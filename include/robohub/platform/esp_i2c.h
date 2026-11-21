/*
 * esp_i2c.h — the I2C HAL on ESP-IDF's I2C master driver (ESP-0012).
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INCLUDE_ROBOHUB_PLATFORM_ESP_I2C_H
#define INCLUDE_ROBOHUB_PLATFORM_ESP_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "error.h"
#include "types.h"
#include "robohub/hal/i2c.h"

/** How many parts one bus talks to: one device handle each. */
#define RH_ESP_I2C_MAX_DEVICES 4

/** One controller on two pins: the interface a driver is handed, first. */
typedef struct RoboHubEspI2C {
    RoboHubI2C interface;
    int        port;
    int        sda;
    int        scl;
    rh_u32     speed_hz;
    void*      bus;                                  /* i2c_master_bus_handle_t */
    rh_u8      addresses[RH_ESP_I2C_MAX_DEVICES];
    void*      devices[RH_ESP_I2C_MAX_DEVICES];      /* i2c_master_dev_handle_t */
} RoboHubEspI2C;

/** Fill in a controller's interface; the bus is made when it is brought up. */
void esp_i2c_create(RoboHubEspI2C* bus, rh_name name, int port, int sda, int scl);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ROBOHUB_PLATFORM_ESP_I2C_H */
