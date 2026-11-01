# RoboHub ESP32 Platform

**Version:** 0.2.0

The ESP32 platform for the RoboHub ecosystem: the interfaces declared by
`robohub-common`, implemented on ESP32 microcontrollers, with board support
packages for the development boards.

## The platform

A Rho program on this platform runs on an ESP32 part. The platform provides
the interfaces, the HAL, and the board support package that registers the
board's peripherals.

## Building

```sh
cd examples/c/hello_world
rho build -target lolin32-lite .
```

## Flashing

```sh
rho flash -target lolin32-lite build/output/hello_world.elf
```

The platform's `tools/esptool/flash` runs the flasher from the vendored IDF's
Python environment, using the bootloader, partition table and application
offsets the build itself wrote. `ESP32_PORT` selects the serial line; a board
that must be put in download mode by hand is flashed with
`ESP32_BEFORE=no-reset` after `GPIO0` has been held low through a reset.

## The examples

`examples/c/hello_world` is the smallest program that runs on real hardware.

`examples/rho/hello_world` is that program in Rho, written against the
contracts and nothing else.

## Deployment metadata

The LOLIN32 Lite's manifest says what a deployment may make of it:

```yaml
capabilities:
  gateway: false
  relay: true
  node: true
  edge: true
  host: false
  wireless: true
  links: "uart console usb wifi socket"
  buses: "uart usb i2c spi onewire gpio adc pwm"

deployment:
  method: "flash"
  arch: "arm"
  kconfig: "CONFIG_RH_BOARD_LOLIN32_LITE=y"
```

The keeper's Mesh planner uses these to offer the board as a node or relay, and
the ESP32's serial console is a link it can serve. The ESP32 is not offered as a
host gateway; that role is the Linux board's.
