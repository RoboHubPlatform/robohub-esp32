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

/* ESP-IDF's console needs nothing polled from a program. */
void robohub_console_task(void)
{
}
