/*
 * bsp.c — LOLIN32 Lite board support.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "bsp.h"
#include "platform.h"
#include "registry.h"
#include "esp_gpio.h"
#include "esp_board_timer.h"
#include "esp_console.h"
#include "error.h"

#ifdef RH_ESP_IDF
#include "driver/gpio.h"
#endif

static RoboHubGPIO led_gpio = {
    .name = "user_led",
    .context = 0,
    .init = esp_gpio_init,
    .set_mode = esp_gpio_set_mode,
    .write = esp_gpio_write,
    .read = esp_gpio_read,
};

static RoboHubTimer system_timer = {
    .name = "system_timer",
    .context = 0,
    .get_millis = esp_timer_get_millis,
    .get_micros = esp_timer_get_micros,
    .delay_ms = esp_timer_delay_ms,
    .delay_us = esp_timer_delay_us,
};

void bsp_register_hals(void)
{
    robohub_register_gpio("user_led", &led_gpio);
    robohub_register_timer("system_timer", &system_timer);
}

RoboHubError robohub_board_register(void)
{
    RoboHubConsoleError cerr = robohub_console_init();
    if (cerr != ROBOHUB_CONSOLE_OK) {
        return ROBOHUB_ERR_IO;
    }
#ifdef RH_ESP_IDF
    gpio_reset_pin((gpio_num_t)RH_LED_PIN);
    gpio_set_direction((gpio_num_t)RH_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)RH_LED_PIN, 1); /* low is on: start off */
#endif
    bsp_register_hals();
    return ROBOHUB_OK;
}

void bsp_init(void)
{
}
