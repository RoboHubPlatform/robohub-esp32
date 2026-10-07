/*
 * esp_tick.c — System tick and delay, on ESP-IDF's own timer.
 *
 * The clock reads the system timer, which ESP-IDF brings up before an
 * application runs. Delays use the ROM's own busy-wait rather than the RTOS
 * scheduler: a delay is the same length either way, and reaching for the
 * scheduler would put a component's generated configuration between this
 * platform and the machine.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "esp_board_timer.h"

#ifdef RH_ESP_IDF
#include "esp_timer.h"      /* the IDF's own, for esp_timer_get_time */
#include "esp_rom_sys.h"
#endif

uint64_t esp_timer_get_millis(RoboHubTimer* self)
{
    (void)self;
#ifdef RH_ESP_IDF
    return (uint64_t)(esp_timer_get_time() / 1000);
#else
    return 0;
#endif
}

uint64_t esp_timer_get_micros(RoboHubTimer* self)
{
    (void)self;
#ifdef RH_ESP_IDF
    return (uint64_t)esp_timer_get_time();
#else
    return 0;
#endif
}

void esp_timer_delay_ms(RoboHubTimer* self, uint32_t ms)
{
    (void)self;
#ifdef RH_ESP_IDF
    esp_rom_delay_us((uint32_t)(ms * 1000u));
#endif
}

void esp_timer_delay_us(RoboHubTimer* self, uint32_t us)
{
    (void)self;
#ifdef RH_ESP_IDF
    esp_rom_delay_us(us);
#endif
}
