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

`examples/c/health_check` walks a board through a watchdog reset, a recorded
panic and a fed watchdog, with a key held in NVS across each reset, and prints
`health-check: passed` (ESP-0007, ESP-0011). `python3
examples/c/health_check/health_check.py` builds it, flashes it and reads the
console (`ESP32_PORT`, `/dev/ttyUSB0` by default). The check's keys replace a key
the board was rotated to, so flash the board from Mesh afterwards.

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
  buses: "uart usb i2c spi gpio"

deployment:
  method: "flash"
  arch: "xtensa"
  kconfig: "CONFIG_RH_BOARD_LOLIN32_LITE=y"
```

The keeper's Mesh planner uses these to offer the board as a node or relay, and
the ESP32's serial console is a link it can serve. The ESP32 is not offered as a
host gateway; that role is the Linux board's.

`buses` lists what the platform has a HAL for — UART, I2C, SPI and GPIO (plus
the USB serial bridge) — and Mesh offers a driver only when the bus it needs is
listed. The ESP32 has 1-Wire-capable pins, ADCs and LEDC PWM, but robohub-esp32
does not implement the `onewire`, `adc` or `pwm` contracts yet, so a DS18B20, an
analog input or a PWM output is not offered on it. `arch` is `xtensa`: the
ESP32's LX6 core, the architecture its target spec (`targets/esp32.json`)
inherits. Mesh reads `deployment.arch` only to cross-build a gateway (it is the
Go `GOARCH`), and the ESP32 is never a gateway, so nothing builds from it.

## The device keystore

The rotated device key (`robohub/keystore.h`) is kept in NVS, namespace
`rh_keys`, as two blobs `a` and `b` holding the A/B records of
`robohub/keystore_core.h` (`src/platform/esp_keystore.c`). If the application
has not initialised NVS, the first keystore call does; if NVS needs erasing
(no free pages, newer format) the keystore does not erase it — that would lose
every key — and reports failure instead. The board's component file names
`rh_keystore_load` and `rh_keystore_store` as undefined symbols on the link, so
the NVS implementation is always the one linked, never the common library's
"no store" default.

On a plain build NVS is readable from flash. NVS encryption protects it, and
NVS encryption needs flash encryption: both are in the production preset.

## Updates over the link

A development build uses ESP-IDF's two-OTA table (`config/sdkconfig.ota`:
`two_ota_large`, application rollback confirmed by the application, 4 MB of
flash). NVS stays at `0x9000` (24 kB), so the keystore's records survive the
move; `ota_0` and `ota_1` are 1700 kB each, and `ota_0` is at `0x20000`, where the
ordinary flash writes the program. That flash also writes the initial
`otadata`, so the next USB flash moves a board to two slots. The manifest says
so (`board.flash.ab: true`), and Mesh records a board it flashed that way.

`src/platform/esp_update.c` is robohub-common's update slot (ESP-0008, robohub
ADR-0024):

- an image is written to the slot not running (`esp_ota_begin` with sequential
  writes, so a sector is erased only when the write reaches it);
- `esp_ota_end` checks the image before it is named for the next boot, and the
  bootloader boots it as `PENDING_VERIFY`;
- the managed layer confirms the image once it is healthy
  (`esp_ota_mark_app_valid_cancel_rollback`) or gives it up
  (`esp_ota_mark_app_invalid_rollback_and_reboot`), and a hang ends the same way
  through the watchdog.

The update's id and versions, its outcome until reported and the board's security
version are one NVS blob (namespace `rh_update`). The build copies the
application image, `<program>.bin`, beside the ELF in the output folder; that
file is what an update sends. A production build keeps its own table and is
flashed only.

## The network

`src/platform/esp_net.c` is robohub-common's network contract (`robohub/net.h`,
ESP-0010):

- **Wi-Fi.** The board joins a network as a station through esp_netif and
  esp_wifi. The first join brings the stack up once (NVS, which is never erased
  here because it holds the keystore; esp_netif; the event loop; the station),
  and the join is polled, with the watchdog fed, until the station has an
  address or the timeout passes.
- **Sockets.** TCP and UDP sockets are lwIP's BSD sockets, non-blocking. A
  receive waits at most its timeout with `select`, and a connect waits at most
  5 s with the watchdog fed. Hosts are numeric, as on the Pico.

The library links ESP-IDF's `esp_wifi`, `esp_netif`, `lwip`, `esp_event`,
`nvs_flash`, `esp_timer` and `freertos` components privately, which also brings
their include directories. The vendored SMP FreeRTOS kernel's headers are left
out of the include globs, because this board's configuration uses the other
kernel.

The console doubles as a link: `robohub_console_write_raw` and
`robohub_console_read_byte` reach UART0 through the ROM's console functions, past
newlib, which turns every `\n` into `\r\n`. So a node's frames and its text
share the CH340, as they share the Pico's USB CDC.

## Production

`CONFIG_RH_PRODUCTION=y` (`production/production.conf`) builds with
`production/sdkconfig.production`: secure boot v2 (RSA-3072), flash encryption
in **release** mode, NVS encryption with its keys in an encrypted `nvs_keys`
partition, and the partition table moved to `0x10000` for the larger bootloader
(`production/partitions.production.csv`). Set `RH_SECURE_BOOT_SIGNING_KEY` to the
signing key (`espsecure.py generate_signing_key --version 2 --scheme rsa3072
key.pem`); the build signs the bootloader and the application. The production
configuration has its own sdkconfig in the build tree, regenerated from the
preset each time, so a tree built before for a plain image cannot keep plain
settings. Secure boot v2 on the original ESP32 needs chip revision v3.0 or
later.

Building burns nothing. **The first boot of a production image burns eFuses**:
the flash encryption key, the secure boot key digest, the download-mode and
JTAG disables. `rho flash --production` (plan: `production/esp32.json`) writes
the signed bootloader without resetting the board, then the partition table and
application, and lists those first-boot eFuse steps before asking for the board
name. Keep the signing key: a board that has booted a production image accepts
only images it signed, and plaintext serial flashing stops working.
