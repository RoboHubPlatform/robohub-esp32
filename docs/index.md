# RoboHub ESP32 Platform

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
rho flash -target lolin32-lite build/output/main
```

## The examples

`examples/c/hello_world` is the smallest program that runs on real hardware.

`examples/rho/hello_world` is that program in Rho, written against the
contracts and nothing else.
