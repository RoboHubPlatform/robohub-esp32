/**
 * @file main.c
 * @brief hello_world — an ESP32 program that blinks and says hello.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"
#include "registry.h"
#include "types.h"
#include "robohub/hal/console.h"
#include "robohub/hal/gpio.h"
#include "robohub/hal/timer.h"

#define BSP_LED_NAME "user_led"
#define BSP_TIMER_NAME "system_timer"

int main(void)
{
    if (robohub_platform_init() != ROBOHUB_OK) {
        return 1;
    }

    RoboHubGPIO* led = robohub_get_gpio(BSP_LED_NAME);
    RoboHubTimer* timer = robohub_get_timer(BSP_TIMER_NAME);

    (void)robohub_console_puts("\nHello world, from an ESP32\n");
    (void)robohub_console_puts("says something every second, from here on\n");
    (void)robohub_console_puts("--------------------------------------\n");

    uint32_t seconds = 0;
    for (;;) {
        seconds++;
        (void)robohub_console_puts("[");
        char digits[12];
        uint32_t count = 0;
        uint32_t value = seconds;
        if (value == 0) {
            digits[count++] = '0';
        }
        while (value > 0) {
            digits[count++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
        while (count > 0) {
            (void)robohub_console_write(&digits[--count], 1);
        }
        (void)robohub_console_puts(" s] hello from the board, in C\n");

        if (led != NULL) {
            (void)led->write(led, 0, seconds % 2u);
        }
        if (timer != NULL) {
            timer->delay_ms(timer, 1000u);
        }
    }
}
