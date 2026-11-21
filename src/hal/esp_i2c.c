/*
 * esp_i2c.c — the I2C HAL on ESP-IDF's I2C master driver (ESP-0012).
 *
 * The bus is made on the pins the board gives when a driver first brings it
 * up, with the internal pull-ups on (a breakout's own are stronger). The driver
 * talks to a device by handle, so each address gets one the first time it is
 * used. Every transfer has a timeout, so a bus held low returns an error rather
 * than stopping the node's loop.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "esp_i2c.h"

#include <string.h>

#ifdef RH_ESP_IDF
#include "driver/i2c_master.h"
#endif

#define ESP_I2C_TIMEOUT_MS 20

static RoboHubError esp_i2c_init(RoboHubI2C* self, rh_u32 speed_hz);
static RoboHubError esp_i2c_write(RoboHubI2C* self, rh_u8 addr, const rh_u8* data, rh_size len);
static RoboHubError esp_i2c_read(RoboHubI2C* self, rh_u8 addr, rh_u8* data, rh_size len);

void esp_i2c_create(RoboHubEspI2C* bus, rh_name name, int port, int sda, int scl)
{
    memset(bus, 0, sizeof(*bus));
    bus->interface.name = name;
    bus->interface.context = bus;
    bus->interface.init = esp_i2c_init;
    bus->interface.write = esp_i2c_write;
    bus->interface.read = esp_i2c_read;
    bus->port = port;
    bus->sda = sda;
    bus->scl = scl;
}

// @req ESP-0012
static RoboHubError esp_i2c_init(RoboHubI2C* self, rh_u32 speed_hz)
{
    if (self == NULL || self->context == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    RoboHubEspI2C* bus = (RoboHubEspI2C*)self->context;
    bus->speed_hz = speed_hz != 0u ? speed_hz : 100000u;
#ifdef RH_ESP_IDF
    if (bus->bus != NULL) {
        return ROBOHUB_OK; /* made once; a second driver on the bus shares it */
    }
    i2c_master_bus_config_t config;
    memset(&config, 0, sizeof(config));
    config.i2c_port = bus->port;
    config.sda_io_num = (gpio_num_t)bus->sda;
    config.scl_io_num = (gpio_num_t)bus->scl;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = 1;
    i2c_master_bus_handle_t handle = NULL;
    if (i2c_new_master_bus(&config, &handle) != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    bus->bus = handle;
#endif
    return ROBOHUB_OK;
}

#ifdef RH_ESP_IDF
/* The device handle for an address, made the first time it is used. */
static i2c_master_dev_handle_t esp_i2c_device(RoboHubEspI2C* bus, rh_u8 addr)
{
    for (int i = 0; i < RH_ESP_I2C_MAX_DEVICES; i++) {
        if (bus->devices[i] != NULL && bus->addresses[i] == addr) {
            return (i2c_master_dev_handle_t)bus->devices[i];
        }
    }
    for (int i = 0; i < RH_ESP_I2C_MAX_DEVICES; i++) {
        if (bus->devices[i] == NULL) {
            i2c_device_config_t config;
            memset(&config, 0, sizeof(config));
            config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
            config.device_address = addr;
            config.scl_speed_hz = bus->speed_hz;
            i2c_master_dev_handle_t handle = NULL;
            if (i2c_master_bus_add_device((i2c_master_bus_handle_t)bus->bus, &config, &handle) != ESP_OK) {
                return NULL;
            }
            bus->addresses[i] = addr;
            bus->devices[i] = handle;
            return handle;
        }
    }
    return NULL;
}

static RoboHubError esp_i2c_result(esp_err_t err)
{
    if (err == ESP_OK) {
        return ROBOHUB_OK;
    }
    return err == ESP_ERR_TIMEOUT ? ROBOHUB_ERR_TIMEOUT : ROBOHUB_ERR_IO;
}
#endif

// @req ESP-0012
static RoboHubError esp_i2c_write(RoboHubI2C* self, rh_u8 addr, const rh_u8* data, rh_size len)
{
    if (self == NULL || self->context == NULL || (data == NULL && len != 0u)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_ESP_IDF
    RoboHubEspI2C* bus = (RoboHubEspI2C*)self->context;
    if (bus->bus == NULL) {
        return ROBOHUB_ERR_STATE;
    }
    i2c_master_dev_handle_t device = esp_i2c_device(bus, addr);
    if (device == NULL) {
        return ROBOHUB_ERR_NO_MEM;
    }
    return esp_i2c_result(i2c_master_transmit(device, data, len, ESP_I2C_TIMEOUT_MS));
#else
    (void)addr;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}

// @req ESP-0012
static RoboHubError esp_i2c_read(RoboHubI2C* self, rh_u8 addr, rh_u8* data, rh_size len)
{
    if (self == NULL || self->context == NULL || data == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
#ifdef RH_ESP_IDF
    RoboHubEspI2C* bus = (RoboHubEspI2C*)self->context;
    if (bus->bus == NULL) {
        return ROBOHUB_ERR_STATE;
    }
    i2c_master_dev_handle_t device = esp_i2c_device(bus, addr);
    if (device == NULL) {
        return ROBOHUB_ERR_NO_MEM;
    }
    return esp_i2c_result(i2c_master_receive(device, data, len, ESP_I2C_TIMEOUT_MS));
#else
    (void)addr;
    (void)len;
    return ROBOHUB_ERR_NOT_SUPPORTED;
#endif
}
