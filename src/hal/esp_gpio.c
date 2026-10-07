/*
 * esp_gpio.c — GPIO HAL implementation for ESP32 boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "esp_gpio.h"

#ifdef RH_ESP_IDF
#include "driver/gpio.h"
#endif

RoboHubError esp_gpio_init(RoboHubGPIO* self)
{
    (void)self;
    return ROBOHUB_OK;
}

RoboHubError esp_gpio_set_mode(RoboHubGPIO* self, uint32_t pin, uint32_t mode)
{
    (void)self;
#ifdef RH_ESP_IDF
    (void)gpio_set_direction((gpio_num_t)pin, (gpio_mode_t)mode);
#endif
    return ROBOHUB_OK;
}

RoboHubError esp_gpio_write(RoboHubGPIO* self, uint32_t pin, uint32_t value)
{
    (void)self;
#ifdef RH_ESP_IDF
    (void)gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
#endif
    return ROBOHUB_OK;
}

RoboHubError esp_gpio_read(RoboHubGPIO* self, uint32_t pin, uint32_t* value)
{
    (void)self;
    if (value == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_ESP_IDF
    *value = (uint32_t)gpio_get_level((gpio_num_t)pin);
#endif
    return ROBOHUB_OK;
}
