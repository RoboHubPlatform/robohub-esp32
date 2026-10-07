/*
 * esp_gpio.h — GPIO HAL for ESP32 boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef ROBOHUB_PLATFORM_ESP_GPIO_H
#define ROBOHUB_PLATFORM_ESP_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "robohub/hal/gpio.h"

RoboHubError esp_gpio_init(RoboHubGPIO* self);
RoboHubError esp_gpio_set_mode(RoboHubGPIO* self, uint32_t pin, uint32_t mode);
RoboHubError esp_gpio_write(RoboHubGPIO* self, uint32_t pin, uint32_t value);
RoboHubError esp_gpio_read(RoboHubGPIO* self, uint32_t pin, uint32_t* value);

#ifdef __cplusplus
}
#endif

#endif /* ROBOHUB_PLATFORM_ESP_GPIO_H */
