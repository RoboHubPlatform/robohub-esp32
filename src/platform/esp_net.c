/**
 * @file esp_net.c
 * @brief robohub-common's network contract on the ESP32: a Wi-Fi station through
 *        esp_netif and esp_wifi, and TCP/UDP sockets through lwIP's BSD API
 *        (ESP-0010, robohub ADR-0025).
 *
 * ESP-IDF runs lwIP in its own FreeRTOS task, so its BSD sockets are available
 * and a receive with a timeout is a `select` bounded by that timeout — the
 * non-blocking wait `robohub/net.h` asks for, without a ring of our own as on the
 * Pico's bare-metal stack.
 *
 * - The first join brings the stack up once: NVS (Wi-Fi keeps calibration
 *   there), esp_netif, the default event loop, the station interface and the
 *   driver. A later join only connects.
 * - A join is polled, with the watchdog fed, until the station has an address or
 *   the timeout passes: a join takes seconds, longer than the core's watchdog
 *   period, and an image on trial must not be reset for joining (ESP-0008).
 * - Hosts are numeric, as on the Pico: the gateway's address is what Mesh writes.
 * - A connect waits at most RH_NET_CONNECT_MS (5 s) with the watchdog fed.
 *
 * @project RoboHub Platform
 * @author Aurel Miron
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

#include "robohub/net.h"

/* Named on the link (--undefined) so this object, and the strong functions
 * below, are always taken from the platform's archive over robohub-common's
 * weak defaults. */
// @req ESP-0010
const char rh_net_platform[] = "esp32";

#ifdef RH_ESP_IDF

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "robohub/health.h"

#ifndef RH_NET_CONNECT_MS
#define RH_NET_CONNECT_MS 5000u
#endif

/* A socket handle is the descriptor plus one, so descriptor 0 is not null. */
#define SOCKET_OF(fd) ((RoboHubSocket)(intptr_t)((fd) + 1))
#define FD_OF(socket) ((int)(intptr_t)(socket) - 1)

static bool net_ready;
static bool net_joined;
static esp_netif_t *station;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* A short wait that keeps the watchdog fed: long waits here are joins and
 * connects, which the core's loop is not running through. */
static void wait_fed(uint32_t ms)
{
    rh_watchdog_feed();
    vTaskDelay(pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1);
}

static RoboHubError stack_up(void)
{
    if (net_ready) {
        return ROBOHUB_OK;
    }
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Not erased here: NVS holds the keystore (esp_keystore.c). Wi-Fi runs
         * without its calibration store. */
        err = ESP_OK;
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return ROBOHUB_ERR_IO;
    }
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return ROBOHUB_ERR_IO;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return ROBOHUB_ERR_IO;
    }
    if (station == 0) {
        station = esp_netif_create_default_wifi_sta();
        if (station == 0) {
            return ROBOHUB_ERR_IO;
        }
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK || esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    net_ready = true;
    return ROBOHUB_OK;
}

static bool has_address(void)
{
    esp_netif_ip_info_t info;
    return station != 0 && esp_netif_get_ip_info(station, &info) == ESP_OK && info.ip.addr != 0;
}

// @req ESP-0010
RoboHubError robohub_net_wifi_connect(const RoboHubWiFiConfig *config)
{
    if (config == 0 || config->ssid == 0 || strlen(config->ssid) >= sizeof(((wifi_sta_config_t *)0)->ssid)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    const char *password = config->password != 0 ? config->password : "";
    if (strlen(password) >= sizeof(((wifi_sta_config_t *)0)->password)) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    rh_watchdog_feed();
    RoboHubError up = stack_up();
    if (up != ROBOHUB_OK) {
        return up;
    }
    wifi_config_t wifi;
    memset(&wifi, 0, sizeof wifi);
    memcpy(wifi.sta.ssid, config->ssid, strlen(config->ssid));
    memcpy(wifi.sta.password, password, strlen(password));
    wifi.sta.threshold.authmode = password[0] != '\0' ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    (void)esp_wifi_disconnect();
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi) != ESP_OK || esp_wifi_connect() != ESP_OK) {
        return ROBOHUB_ERR_IO;
    }
    uint32_t timeout = config->timeout_ms != 0u ? config->timeout_ms : 15000u;
    uint32_t start = now_ms();
    while (!has_address()) {
        if (now_ms() - start >= timeout) {
            (void)esp_wifi_disconnect();
            return ROBOHUB_ERR_TIMEOUT;
        }
        wait_fed(50);
    }
    net_joined = true;
    return ROBOHUB_OK;
}

// @req ESP-0010
RoboHubError robohub_net_wifi_disconnect(void)
{
    if (!net_ready) {
        return ROBOHUB_OK;
    }
    net_joined = false;
    return esp_wifi_disconnect() == ESP_OK ? ROBOHUB_OK : ROBOHUB_ERR_IO;
}

// @req ESP-0010
RoboHubNetStatus robohub_net_status(void)
{
    if (!net_ready || !net_joined) {
        return ROBOHUB_NET_DISCONNECTED;
    }
    return has_address() ? ROBOHUB_NET_CONNECTED : ROBOHUB_NET_CONNECTING;
}

// @req ESP-0010
RoboHubError robohub_net_ip(char *out_ip, rh_size max_len)
{
    if (out_ip == 0 || max_len < 8u) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    esp_netif_ip_info_t info;
    if (station == 0 || esp_netif_get_ip_info(station, &info) != ESP_OK || info.ip.addr == 0) {
        return ROBOHUB_ERR_STATE;
    }
    return esp_ip4addr_ntoa(&info.ip, out_ip, (int)max_len) != 0 ? ROBOHUB_OK : ROBOHUB_ERR_NO_MEM;
}

// @req ESP-0010
RoboHubSocket robohub_socket_create(RoboHubSocketType type)
{
    int fd = lwip_socket(AF_INET, type == ROBOHUB_SOCKET_UDP ? SOCK_DGRAM : SOCK_STREAM,
                         type == ROBOHUB_SOCKET_UDP ? IPPROTO_UDP : IPPROTO_TCP);
    if (fd < 0) {
        return 0;
    }
    int flags = lwip_fcntl(fd, F_GETFL, 0);
    (void)lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return SOCKET_OF(fd);
}

// @req ESP-0010
RoboHubError robohub_socket_connect(RoboHubSocket socket, const char *host, rh_u16 port)
{
    if (socket == 0 || host == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    struct sockaddr_in to;
    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    if (inet_aton(host, &to.sin_addr) == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    int fd = FD_OF(socket);
    if (lwip_connect(fd, (struct sockaddr *)&to, sizeof to) == 0) {
        return ROBOHUB_OK;
    }
    if (errno != EINPROGRESS) {
        return ROBOHUB_ERR_IO;
    }
    /* Waited for in short steps with the watchdog fed. */
    uint32_t start = now_ms();
    while (now_ms() - start < RH_NET_CONNECT_MS) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        struct timeval step = {0, 100000};
        rh_watchdog_feed();
        int ready = lwip_select(fd + 1, 0, &writable, 0, &step);
        if (ready < 0) {
            return ROBOHUB_ERR_IO;
        }
        if (ready > 0) {
            int error = 0;
            socklen_t length = sizeof error;
            if (lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error != 0) {
                return ROBOHUB_ERR_IO;
            }
            return ROBOHUB_OK;
        }
    }
    return ROBOHUB_ERR_TIMEOUT;
}

// @req ESP-0010
int robohub_socket_send(RoboHubSocket socket, const void *data, rh_size len)
{
    if (socket == 0 || (data == 0 && len != 0u)) {
        return ROBOHUB_SOCKET_ERROR;
    }
    int fd = FD_OF(socket);
    rh_size sent = 0;
    uint32_t start = now_ms();
    /* All of it, or an error: a frame half sent is a broken stream. */
    while (sent < len) {
        int n = lwip_send(fd, (const char *)data + sent, len - sent, 0);
        if (n > 0) {
            sent += (rh_size)n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && now_ms() - start < RH_NET_CONNECT_MS) {
            wait_fed(5);
            continue;
        }
        return ROBOHUB_SOCKET_ERROR;
    }
    return (int)sent;
}

// @req ESP-0010
int robohub_socket_recv(RoboHubSocket socket, void *data, rh_size len, rh_u32 timeout_ms)
{
    if (socket == 0 || data == 0 || len == 0u) {
        return ROBOHUB_SOCKET_ERROR;
    }
    int fd = FD_OF(socket);
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(fd, &readable);
    struct timeval wait = {(long)(timeout_ms / 1000u), (long)((timeout_ms % 1000u) * 1000u)};
    int ready = lwip_select(fd + 1, &readable, 0, 0, &wait);
    if (ready < 0) {
        return ROBOHUB_SOCKET_ERROR;
    }
    if (ready == 0) {
        return ROBOHUB_SOCKET_TIMEOUT;
    }
    int n = lwip_recv(fd, data, len, 0);
    if (n > 0) {
        return n;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return ROBOHUB_SOCKET_TIMEOUT;
    }
    return ROBOHUB_SOCKET_ERROR; /* 0: the peer closed */
}

// @req ESP-0010
void robohub_socket_close(RoboHubSocket socket)
{
    if (socket != 0) {
        (void)lwip_close(FD_OF(socket));
    }
}

#endif /* RH_ESP_IDF */
