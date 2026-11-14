/**
 * @file esp_keystore.c
 * @brief The device keystore in NVS: namespace "rh_keys", blobs "a" and "b".
 *
 * NVS already writes atomically, but the A/B records with their CRC32 and
 * sequence numbers are kept anyway (robohub/keystore_core.h): a board on any
 * platform stores and chooses its key by the same rules, and a corrupt entry is
 * detected the same way.
 *
 * NVS lives in the `nvs` partition of the default partition table. If the
 * application has not initialised it, the first load or store does
 * (nvs_flash_init). If NVS needs erasing (no free pages, or a newer format) this
 * file does NOT erase it — that would throw away every stored key along with
 * the rest of the partition — and the store simply reports failure.
 *
 * NVS is plain text on the flash unless NVS encryption is on, and NVS
 * encryption needs flash encryption: both are part of the ESP32 production
 * preset (presets/production/), never on by default because enabling flash
 * encryption in release mode burns eFuses.
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/keystore.h"
#include "robohub/keystore_core.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive; see keystore_core.h. */
const char rh_keystore_platform[] = "esp32";

#ifdef RH_ESP_IDF

#include "nvs.h"
#include "nvs_flash.h"

#define RH_KEYSTORE_NAMESPACE "rh_keys"

static const char *const slot_names[RH_KEYSTORE_SLOTS] = { "a", "b" };

static int esp_open(nvs_open_mode_t mode, nvs_handle_t *handle)
{
    esp_err_t err = nvs_open(RH_KEYSTORE_NAMESPACE, mode, handle);
    if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
        if (nvs_flash_init() != ESP_OK) {
            return -1;
        }
        err = nvs_open(RH_KEYSTORE_NAMESPACE, mode, handle);
    }
    return err == ESP_OK ? 0 : -1;
}

static int esp_read(void *ctx, unsigned slot, uint8_t *buf, size_t len)
{
    (void)ctx;
    nvs_handle_t handle;
    if (slot >= RH_KEYSTORE_SLOTS || esp_open(NVS_READONLY, &handle) != 0) {
        return -1;
    }
    size_t size = len;
    esp_err_t err = nvs_get_blob(handle, slot_names[slot], buf, &size);
    nvs_close(handle);
    return (err == ESP_OK && size == len) ? 0 : -1;
}

static int esp_erase(void *ctx, unsigned slot)
{
    (void)ctx;
    nvs_handle_t handle;
    if (slot >= RH_KEYSTORE_SLOTS || esp_open(NVS_READWRITE, &handle) != 0) {
        return -1;
    }
    esp_err_t err = nvs_erase_key(handle, slot_names[slot]);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK ? 0 : -1;
}

static int esp_write(void *ctx, unsigned slot, const uint8_t *buf, size_t len)
{
    (void)ctx;
    nvs_handle_t handle;
    if (slot >= RH_KEYSTORE_SLOTS || esp_open(NVS_READWRITE, &handle) != 0) {
        return -1;
    }
    esp_err_t err = nvs_set_blob(handle, slot_names[slot], buf, len);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK ? 0 : -1;
}

static const rh_keystore_backend_t esp_backend = { 0, esp_read, esp_erase, esp_write };

int rh_keystore_load(rh_key_t *out)
{
    return rh_keystore_core_load(&esp_backend, out);
}

int rh_keystore_store(const rh_key_t *key)
{
    return rh_keystore_core_store(&esp_backend, key);
}

#endif /* RH_ESP_IDF */
