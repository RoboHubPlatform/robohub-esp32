/*
 * esp_platform.h — Platform initialization for ESP32 boards.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef ROBOHUB_PLATFORM_ESP_PLATFORM_H
#define ROBOHUB_PLATFORM_ESP_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"

RoboHubError esp_platform_init(void);

#ifdef __cplusplus
}
#endif

#endif /* ROBOHUB_PLATFORM_ESP_PLATFORM_H */
