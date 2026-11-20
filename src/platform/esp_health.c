/**
 * @file esp_health.c
 * @brief robohub-common's health contracts on the ESP32 (ESP-0007).
 *
 * - **Watchdog**: ESP-IDF's task watchdog, through a *user* subscription
 *   (`esp_task_wdt_add_user`), so it is fed from whichever task runs the node's
 *   loop rather than the task that started it. It panics when it runs out, and
 *   its interrupt calls `esp_task_wdt_isr_user_handler` first: that is the early
 *   warning, and it writes the record. Any period in milliseconds; it can be
 *   stopped (the user is removed).
 * - **Reset reason**: `esp_reset_reason()`, mapped onto the portable set — the
 *   interrupt, task and other watchdogs are `watchdog`, a panic or a CPU lock-up
 *   a `fault`, a brown-out or power glitch `brown-out`.
 * - **Retained memory**: 32 bytes of RTC memory a reset keeps (`RTC_NOINIT_ATTR`).
 * - **Faults**: the panic handler is wrapped (`-Wl,--wrap=esp_panic_handler`,
 *   ESP-IDF's documented hook) to write the record with the faulting address
 *   before ESP-IDF's own handler prints and reboots.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/health.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive over robohub-common's
 * weak defaults. */
// @req ESP-0007
const char rh_health_platform[] = "esp32";

#ifdef RH_ESP_IDF

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_private/panic_internal.h"

/* ESP-IDF's task watchdog API (esp_task_wdt.h, ESP-IDF 5.0 on). Its header
 * includes FreeRTOS's, which need the port and the kernel ESP-IDF chose for the
 * project; this library is linked by the generated project rather than built as
 * an ESP-IDF component, and does not see that choice. The five calls it uses
 * are declared here with ESP-IDF's own signatures instead. */
typedef struct {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
} esp_task_wdt_config_t;
typedef struct esp_task_wdt_user_handle_s *esp_task_wdt_user_handle_t;
esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config);
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config);
esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret);
esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle);
esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t user_handle);

static RTC_NOINIT_ATTR uint32_t esp_health_retained[RH_HEALTH_RETAINED_SIZE / 4u];
static esp_task_wdt_user_handle_t esp_wdt_user;
static bool esp_wdt_running;

// @req ESP-0007
void rh_watchdog_caps_get(rh_watchdog_caps *caps)
{
    if (caps) {
        caps->present = true;
        caps->stoppable = true;
        caps->early_warning = true;
        caps->min_ms = 1u;
        caps->max_ms = 0xFFFFFFFFu;
    }
}

// @req ESP-0007
RoboHubError rh_watchdog_start(uint32_t period_ms, uint32_t *chosen_ms)
{
    if (period_ms == 0u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    esp_task_wdt_config_t config = {
        .timeout_ms = period_ms,
        .idle_core_mask = 0u,   /* the node's loop is what is watched, not the idle tasks */
        .trigger_panic = true,
    };
    /* ESP-IDF may have started the task watchdog itself (CONFIG_ESP_TASK_WDT_INIT). */
    esp_err_t err = esp_task_wdt_reconfigure(&config);
    if (err == ESP_ERR_INVALID_STATE) {
        err = esp_task_wdt_init(&config);
    }
    if (err != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    if (!esp_wdt_running) {
        if (esp_task_wdt_add_user("robohub", &esp_wdt_user) != ESP_OK) {
            return ROBOHUB_ERR_IO;
        }
        esp_wdt_running = true;
    }
    if (chosen_ms) {
        *chosen_ms = period_ms;
    }
    return ROBOHUB_OK;
}

// @req ESP-0007
void rh_watchdog_feed(void)
{
    if (esp_wdt_running) {
        (void)esp_task_wdt_reset_user(esp_wdt_user);
    }
}

// @req ESP-0007
RoboHubError rh_watchdog_stop(void)
{
    if (esp_wdt_running) {
        if (esp_task_wdt_delete_user(esp_wdt_user) != ESP_OK) {
            return ROBOHUB_ERR_IO;
        }
        esp_wdt_running = false;
    }
    return ROBOHUB_OK;
}

// @req ESP-0007
rh_reset_reason rh_platform_reset_reason(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:    return RH_RESET_POWER_ON;
    case ESP_RST_EXT:        return RH_RESET_PIN;
    case ESP_RST_SW:         return RH_RESET_SOFTWARE;
    case ESP_RST_DEEPSLEEP:  return RH_RESET_SOFTWARE;  /* the program slept and woke */
    case ESP_RST_USB:        return RH_RESET_SOFTWARE;  /* a host reset it over USB (esptool) */
    case ESP_RST_JTAG:       return RH_RESET_SOFTWARE;  /* a debugger asked */
    case ESP_RST_PANIC:      return RH_RESET_FAULT;
    case ESP_RST_CPU_LOCKUP: return RH_RESET_FAULT;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:        return RH_RESET_WATCHDOG;
    case ESP_RST_BROWNOUT:
    case ESP_RST_PWR_GLITCH: return RH_RESET_BROWN_OUT;
    default:                 return RH_RESET_UNKNOWN;
    }
}

// @req ESP-0007
void *rh_platform_retained(void)
{
    return esp_health_retained;
}

/* The task watchdog ran out: its ISR calls this before it panics. A task-
 * watchdog panic then also passes the wrapped panic handler below, which would
 * overwrite this record with a hard fault — so it keeps a watchdog record. */
static volatile bool esp_wdt_fired;

// @req ESP-0007
void esp_task_wdt_isr_user_handler(void)
{
    esp_wdt_fired = true;
    rh_health_fault(RH_FAULT_WATCHDOG, 0u);
}

void __real_esp_panic_handler(panic_info_t *info);

// @req ESP-0007
void __wrap_esp_panic_handler(panic_info_t *info)
{
    if (!esp_wdt_fired) {
        rh_health_fault(RH_FAULT_HARD, info != 0 ? (uint32_t)(uintptr_t)info->addr : 0u);
    }
    __real_esp_panic_handler(info);
}

#endif /* RH_ESP_IDF */
