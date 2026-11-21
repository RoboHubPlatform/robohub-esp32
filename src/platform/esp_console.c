/*
 * esp_console.c — Console contract for ESP32 boards.
 *
 * The console is ESP-IDF's: its newlib retarget writes to the UART the board
 * was configured with, which on these boards is UART0 on the pins the ROM
 * prints to and the USB-serial chip is wired to. This file only says that the
 * platform's console writes there too, so a `printf` in a program and a write
 * through this contract reach the same place.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "esp_console.h"

#include <stdio.h>

#ifdef RH_ESP_IDF
#include "esp_rom_serial_output.h"
#endif

RoboHubConsole robohub_console;

RoboHubConsoleError robohub_console_init(void)
{
    if (robohub_console.ready) {
        return ROBOHUB_CONSOLE_OK;
    }
    robohub_console.ready = 1u;
    robohub_console.written = 0u;
    return ROBOHUB_CONSOLE_OK;
}

RoboHubConsoleError robohub_console_write(const char* data, size_t len)
{
    if (data == NULL || len == 0) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    if (!robohub_console.ready) {
        return ROBOHUB_CONSOLE_ERR_STATE;
    }
    (void)fwrite(data, 1, len, stdout);
    robohub_console.written += (uint64_t)len;
    return ROBOHUB_CONSOLE_OK;
}

RoboHubConsoleError robohub_console_puts(const char* text)
{
    if (text == NULL) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    size_t len = 0;
    while (text[len] != '\0') {
        len++;
    }
    return robohub_console_write(text, len);
}

/* A frame, not a line: written past newlib, whose console turns every `\n`
 * into `\r\n` and buffers, so a link and the text console can share UART0 as
 * they share the Pico's USB CDC. What `printf` buffered goes first, so the two
 * leave in the order they were written. */
// @req RNODE-TPL-0002
RoboHubConsoleError robohub_console_write_raw(const uint8_t* data, size_t len)
{
    if (data == NULL || len == 0) {
        return ROBOHUB_CONSOLE_ERR_INVALID;
    }
    if (!robohub_console.ready) {
        return ROBOHUB_CONSOLE_ERR_STATE;
    }
#ifdef RH_ESP_IDF
    (void)fflush(stdout);
    for (size_t i = 0; i < len; i++) {
        (void)esp_rom_output_tx_one_char(data[i]);
    }
#else
    (void)fwrite(data, 1, len, stdout);
#endif
    robohub_console.written += (uint64_t)len;
    return ROBOHUB_CONSOLE_OK;
}

/* One byte from UART0's receive FIFO, or -1: polled, so a loop can drain the
 * console as a link without waiting on it. */
// @req RNODE-TPL-0002
int robohub_console_read_byte(void)
{
#ifdef RH_ESP_IDF
    uint8_t byte = 0;
    return esp_rom_output_rx_one_char(&byte) == 0 ? (int)byte : -1;
#else
    return -1;
#endif
}

/* ESP-IDF's console needs nothing polled from a program. */
void robohub_console_task(void)
{
}
