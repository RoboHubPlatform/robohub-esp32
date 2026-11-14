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
