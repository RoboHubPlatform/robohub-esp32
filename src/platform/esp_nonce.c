/**
 * @file esp_nonce.c
 * @brief The platform's nonce source: the ESP32's hardware random generator.
 *
 * esp_fill_random reads the RNG peripheral, which is a true random source while
 * the radio is on and is seeded from the bootloader's entropy otherwise. See
 * robohub_platform_nonce in platform.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "platform.h"

#include "esp_random.h"

int robohub_platform_nonce(rh_u8* out, rh_size length)
{
    esp_fill_random(out, (size_t)length);
    return 0;
}
