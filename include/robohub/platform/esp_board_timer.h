/*
 * esp_timer.h — Timer HAL for ESP32 boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef ROBOHUB_PLATFORM_ESP_TIMER_H
#define ROBOHUB_PLATFORM_ESP_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "robohub/hal/timer.h"

uint64_t esp_timer_get_millis(RoboHubTimer* self);
uint64_t esp_timer_get_micros(RoboHubTimer* self);
void esp_timer_delay_ms(RoboHubTimer* self, uint32_t ms);
void esp_timer_delay_us(RoboHubTimer* self, uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* ROBOHUB_PLATFORM_ESP_TIMER_H */
