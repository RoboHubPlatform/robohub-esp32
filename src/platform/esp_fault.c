/*
 * esp_fault.c — What a fault says on an ESP32.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifdef RH_ESP_IDF
#include "esp_system.h"
#endif

void esp_fault_register(void)
{
    /* ESP-IDF installs its own panic handler and its own backtrace, which
     * says more than anything this could print. Nothing to add. */
}
