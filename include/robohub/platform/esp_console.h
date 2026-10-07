/*
 * esp_console.h — Console contract for ESP32 boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef ROBOHUB_PLATFORM_ESP_CONSOLE_H
#define ROBOHUB_PLATFORM_ESP_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "robohub/hal/console.h"

RoboHubConsoleError esp_console_init(void);
RoboHubConsoleError esp_console_write(const char* data, size_t len);
RoboHubConsoleError esp_console_puts(const char* text);
void robohub_console_task(void);

#ifdef __cplusplus
}
#endif

#endif /* ROBOHUB_PLATFORM_ESP_CONSOLE_H */
