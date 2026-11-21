/**
 * @file esp_update.c
 * @brief robohub-common's update slot on the ESP32: ESP-IDF's two OTA slots and
 *        application rollback, confirmed by the application (ESP-0008, robohub
 *        ADR-0024).
 *
 * - An image is written to the OTA slot not running (`esp_ota_begin` with
 *   sequential writes, so a sector is erased as it is reached rather than the
 *   whole slot at once, which would outlast the core's watchdog).
 * - rh_update_boot_once() validates the image (`esp_ota_end`), names its slot
 *   for the next boot, records the update in NVS and restarts. The bootloader
 *   boots it as PENDING_VERIFY.
 * - Pending: the running slot is PENDING_VERIFY and is the one recorded.
 * - rh_update_confirm(): `esp_ota_mark_app_valid_cancel_rollback`.
 * - rh_update_reject(): `esp_ota_mark_app_invalid_rollback_and_reboot`. A hang
 *   ends the same way: the core's watchdog resets the board, and the bootloader
 *   marks the unconfirmed image ABORTED and boots the one before.
 * - Any boot that finds the record pending but runs another slot calls it
 *   rolled back; one that finds the recorded slot running and valid — confirmed,
 *   with the record not yet updated (a power cut between) — calls it kept.
 *
 * The record — the update's id and versions, its outcome until taken, the
 * board's security version — is one NVS blob beside the keystore's, in the NVS
 * partition the two-slot layout keeps where it was.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/update_slot.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive. */
// @req ESP-0008
const char rh_update_platform[] = "esp32";

#ifdef RH_ESP_IDF

#include <string.h>

#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_rom_sys.h"
#include "nvs.h"
#include "nvs_flash.h"

#define ESP_UPDATE_NAMESPACE "rh_update"
#define ESP_UPDATE_KEY "state"

typedef struct {
    uint32_t version;           /* 1 */
    uint32_t pending;           /* an update was booted once and not settled */
    uint32_t pending_address;   /* the flash address of its slot */
    rh_update_info info;
    uint32_t outcome;           /* rh_update_outcome, until taken */
    rh_update_info outcome_info;
    uint32_t security;
} esp_update_state;

static esp_update_state state;
static bool state_loaded;
static bool settled;

static const esp_partition_t *write_slot;
static esp_ota_handle_t write_handle;
static uint32_t write_size, written;

static int nvs_handle_open(nvs_open_mode_t mode, nvs_handle_t *handle)
{
    esp_err_t err = nvs_open(ESP_UPDATE_NAMESPACE, mode, handle);
    if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
        if (nvs_flash_init() != ESP_OK) {
            return -1;
        }
        err = nvs_open(ESP_UPDATE_NAMESPACE, mode, handle);
    }
    return err == ESP_OK ? 0 : -1;
}

static void state_load(void)
{
    if (state_loaded) {
        return;
    }
    state_loaded = true;
    memset(&state, 0, sizeof state);
    nvs_handle_t handle;
    if (nvs_handle_open(NVS_READONLY, &handle) != 0) {
        return;
    }
    size_t size = sizeof state;
    if (nvs_get_blob(handle, ESP_UPDATE_KEY, &state, &size) != ESP_OK || size != sizeof state || state.version != 1u) {
        memset(&state, 0, sizeof state);
    }
    nvs_close(handle);
}

static int state_save(void)
{
    nvs_handle_t handle;
    if (nvs_handle_open(NVS_READWRITE, &handle) != 0) {
        return -1;
    }
    state.version = 1u;
    esp_err_t err = nvs_set_blob(handle, ESP_UPDATE_KEY, &state, sizeof state);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK ? 0 : -1;
}

static bool running_pending_verify(const esp_partition_t **running_out)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running_out) {
        *running_out = running;
    }
    esp_ota_img_states_t img = ESP_OTA_IMG_UNDEFINED;
    return running != 0 && esp_ota_get_state_partition(running, &img) == ESP_OK && img == ESP_OTA_IMG_PENDING_VERIFY;
}

/* Once per boot: is this the recorded update's trial, or a boot after one
 * that settled without saying so? */
// @req ESP-0008
static void settle(void)
{
    if (settled) {
        return;
    }
    settled = true;
    state_load();
    if (!state.pending) {
        return;
    }
    const esp_partition_t *running = 0;
    bool trial = running_pending_verify(&running);
    if (running != 0 && running->address == state.pending_address) {
        if (trial) {
            return; /* on trial: rh_update_pending */
        }
        /* Confirmed, and the record not updated before a reset. */
        if (state.info.security_version > state.security) {
            state.security = state.info.security_version;
        }
        state.outcome = RH_UPDATE_KEPT;
    } else {
        state.outcome = RH_UPDATE_ROLLED_BACK;
    }
    state.outcome_info = state.info;
    state.pending = 0;
    (void)state_save();
}

// @req ESP-0008
void rh_update_caps_get(rh_update_caps *caps)
{
    if (caps == 0) {
        return;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(0);
    caps->updatable = running != 0 && next != 0 && next != running &&
                      next->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
                      next->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX;
    caps->slot_size = caps->updatable ? next->size : 0u;
}

// @req ESP-0008
RoboHubError rh_update_begin(uint32_t size)
{
    settle();
    rh_update_caps caps;
    rh_update_caps_get(&caps);
    if (!caps.updatable) {
        return ROBOHUB_ERR_NOT_SUPPORTED;
    }
    if (size == 0u || size > caps.slot_size) {
        return ROBOHUB_ERR_NO_MEM;
    }
    if (write_slot != 0) {
        (void)esp_ota_abort(write_handle); /* a transfer begun again forgets the last */
        write_slot = 0;
    }
    const esp_partition_t *next = esp_ota_get_next_update_partition(0);
    if (esp_ota_begin(next, OTA_WITH_SEQUENTIAL_WRITES, &write_handle) != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    write_slot = next;
    write_size = size;
    written = 0u;
    return ROBOHUB_OK;
}

// @req ESP-0008
RoboHubError rh_update_write(uint32_t offset, const uint8_t *data, uint32_t length)
{
    if (write_slot == 0 || offset != written) {
        return ROBOHUB_ERR_STATE;
    }
    if (data == 0 || length > write_size - written) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (esp_ota_write(write_handle, data, length) != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    written += length;
    return ROBOHUB_OK;
}

// @req ESP-0008
RoboHubError rh_update_read(uint32_t offset, uint8_t *out, uint32_t length)
{
    if (write_slot == 0) {
        return ROBOHUB_ERR_STATE;
    }
    if (out == 0 || offset > written || length > written - offset) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    return esp_partition_read(write_slot, offset, out, length) == ESP_OK ? ROBOHUB_OK : ROBOHUB_ERR_IO;
}

// @req ESP-0008
RoboHubError rh_update_boot_once(const rh_update_info *info)
{
    if (write_slot == 0 || written != write_size || info == 0) {
        return ROBOHUB_ERR_STATE;
    }
    /* ESP-IDF checks the image itself — its header, its checksum and hash —
     * before it may be booted. */
    esp_err_t err = esp_ota_end(write_handle);
    const esp_partition_t *slot = write_slot;
    write_slot = 0;
    if (err != ESP_OK || esp_ota_set_boot_partition(slot) != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    state_load();
    state.pending = 1u;
    state.pending_address = slot->address;
    state.info = *info;
    if (state_save() != 0) {
        return ROBOHUB_ERR_IO;
    }
    /* A quarter of a second, so the `verified` just sent leaves first. */
    esp_rom_delay_us(250000u);
    esp_restart();
    return ROBOHUB_ERR_STATE;
}

// @req ESP-0008
bool rh_update_pending(rh_update_info *out)
{
    settle();
    if (!state.pending) {
        return false;
    }
    if (out != 0) {
        *out = state.info;
    }
    return true;
}

// @req ESP-0008
RoboHubError rh_update_confirm(void)
{
    if (!rh_update_pending(0)) {
        return ROBOHUB_ERR_STATE;
    }
    if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    if (state.info.security_version > state.security) {
        state.security = state.info.security_version;
    }
    state.outcome = RH_UPDATE_KEPT;
    state.outcome_info = state.info;
    state.pending = 0u;
    return state_save() == 0 ? ROBOHUB_OK : ROBOHUB_ERR_IO;
}

// @req ESP-0008
RoboHubError rh_update_reject(void)
{
    if (!rh_update_pending(0)) {
        return ROBOHUB_ERR_STATE;
    }
    /* Marks the image invalid and reboots into the one before; the next boot
     * finds the record pending on another slot and calls it rolled back. */
    (void)esp_ota_mark_app_invalid_rollback_and_reboot();
    return ROBOHUB_ERR_IO; /* it did not reboot */
}

// @req ESP-0008
rh_update_outcome rh_update_take_outcome(rh_update_info *out)
{
    settle();
    rh_update_outcome outcome = (rh_update_outcome)state.outcome;
    if (outcome == RH_UPDATE_NONE) {
        return RH_UPDATE_NONE;
    }
    if (out != 0) {
        *out = state.outcome_info;
    }
    state.outcome = RH_UPDATE_NONE;
    (void)state_save();
    return outcome;
}

// @req ESP-0008
uint32_t rh_update_security_version(void)
{
    state_load();
    return state.security;
}

// @req ESP-0008
int rh_update_running_slot(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == 0 || running->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MIN ||
        running->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
        return -1;
    }
    return (int)(running->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
}

#endif /* RH_ESP_IDF */
