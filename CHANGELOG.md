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
