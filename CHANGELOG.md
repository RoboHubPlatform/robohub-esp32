# Changelog

All notable changes to this module are recorded here, one section per release.

## [0.2.0] - 2026-11-01

The LOLIN32 Lite declares the relay/node/edge roles, WiFi and UART/USB links,
its buses and its flash deployment, so a Mesh planner offers it. Its esptool
flash entry writes the bootloader, partition table and application the build
recorded in `flash_args`.

## [0.3.0] - 2026-11-14

The board key is kept in NVS. A production preset turns on secure boot v2, flash encryption and NVS encryption; the eFuse steps are irreversible and run only through `rho flash --production` with confirmation.

## [0.3.1] - 2026-11-19

The LOLIN32 Lite's manifest said `deployment.arch: "arm"` for an Xtensa part; it is `xtensa`, the arch its target spec inherits. Mesh reads that value only as the `GOARCH` of a gateway it cross-builds, and the ESP32 is never a gateway, so no build changes. Its `capabilities.buses` claimed `onewire`, `adc` and `pwm`, for which the platform has no HAL (it implements GPIO, I2C, SPI, UART, timer and RS-485), so Mesh offered DS18B20, analog and PWM drivers on a board that could not run them; the claims are removed (`uart usb i2c spi gpio`) and the documentation says why.

## [0.4.0] - 2026-11-20

The health contracts on the ESP32 (ESP-0007, robohub-common 1.3.0): ESP-IDF's task watchdog through a user subscription (fed from whichever task runs the loop; its ISR hook is the early warning that writes the record), `esp_reset_reason()` mapped onto the portable reasons, 32 bytes of `RTC_NOINIT` memory for the fault record and boot count, and ESP-IDF's panic handler wrapped (`--wrap=esp_panic_handler`) to record the faulting address before it reboots. The five task-watchdog calls are declared with ESP-IDF's signatures, since its header needs the FreeRTOS port this library does not build against.
