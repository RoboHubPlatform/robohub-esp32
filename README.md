# robohub-esp32

The ESP32 platform for the [RoboHub Platform](../robohub-common).

`robohub-common` declares the platform-independent interfaces — the HAL contracts
for GPIO, I2C, SPI, UART, RS485 and Timer, the error codes, the instance
registry, and the platform initialisation contract. It contains no hardware code
and knows nothing about any particular chip. This module is the other half: it
implements those interfaces against Espressif's ESP-IDF, and carries a board
support package for each supported board.

An application depends on both. It writes against the interfaces, and the two
modules together are what a build links:

```yaml
depends:
  - name: robohub-common
    type: c
    path: ../robohub-common
  - name: robohub-esp32
    type: c
    path: ../robohub-esp32
```

## What is here

| Path | What it is |
|------|------------|
| `include/robohub/platform/` | The platform's own headers — what a board or a driver includes: `esp_platform.h`, `esp_console.h`, `esp_gpio.h`, `esp_board_timer.h` |
| `src/platform/` | Chip-independent platform code: console, fault handling, platform init and the system tick |
| `src/hal/` | The HAL implementations, one file per interface: `esp_gpio.c`, `esp_i2c.c`, `esp_spi.c`, `esp_uart.c`, `esp_timer.c`, `esp_rs485.c` |
| `boards/<board>/` | One BSP per development board: its pin map, its LED, its registration, and the prelude that makes the build ESP-IDF's |
| `kconfig/` | Kconfig fragments for the symbols this platform owns |
| `targets/` | Target specs: the `esp32` chip and the `xtensa` architecture it inherits its toolchain from |
| `presets/` | Preset specs, naming the vendor repository a board of this family is built against |
| `tests/` | The project's host tests |
| `examples/` | Hello-world programs, one per language the toolchain can build |
| `vendor/` | The vendored ESP-IDF, fetched by a build. Nothing here is checked in |

## Boards

This module currently supports one board:

| Board | Part | Flash | Console |
|---|---|---|---|
| `lolin32-lite` | ESP32-WROOM-32 (Xtensa LX6, two cores, 520 KB SRAM) | 4 MB | UART0 over a CH340 USB-serial chip, 115200 baud |

The LOLIN32 Lite's BSP lives in `boards/lolin32-lite/`. It maps the on-board LED
(on GPIO5, lit when the pin is low), the default UART and the I2C and SPI buses,
and registers the `user_led` and `system_timer` instances against the common
registry.

`hardware.supported` in `module.yml` is the list of boards a build is claimed to
work for. A board directory that is absent from that list is not claimed to
build.

## Dependencies

- [robohub-common](https://github.com/RoboHubPlatform/robohub-common) — the HAL
  contracts, error model and registry this platform implements.
- [ESP-IDF](https://github.com/espressif/esp-idf) — Espressif's official
  framework, vendored beside the platform under `vendor/esp-idf`. ESP-IDF owns
  the flags, the second-stage bootloader, the partition table and the per-memory
  linker fragments; this platform does not reimplement any of them.

## Building

This is a C library, not a command, so it is consumed by a project rather than
run. Built through the toolchain, which fetches the vendor tree and generates
the files ESP-IDF asks for and does not ship:

```sh
rho build -target lolin32-lite examples/c/hello_world
rho test                        # the tests this project declares
```

Or with CMake on its own, which is what a person does when they are not going to
use the toolchain:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

A bare-metal ESP32 build is ESP-IDF's build. The board's prelude
(`boards/lolin32-lite/idf.cmake`) locates the framework before `project()` and
hands the build to it. It looks for ESP-IDF in this order:

1. `IDF_PATH`, if the environment sets it;
2. `vendor/esp-idf` beside the platform;
3. otherwise the build reports that the SDK is missing, and host builds and
   tests still work while a bare-metal build will not link.

ESP-IDF's own build scripts run under the Python environment its installer
creates, found under `~/.espressif/python_env/idf*_py*_env`. If there is none,
run the vendored IDF's installer once:

```sh
vendor/esp-idf/install.sh esp32
```

`robohub-common`'s headers are needed to compile anything here. CMake finds them
in a sibling checkout or at `/home/miraur/projects/robohub-common`; otherwise
set `ROBOHUB_COMMON_DIR` to point at the checkout.

## Flashing

An ESP32 image is three binaries at three offsets, so the platform's
`tools/esptool/flash` entry point does not invent a command line: it runs the
`flash_args` file the ESP-IDF build wrote, from the build directory those paths
are relative to. The flasher is the `esptool` inside the IDF's own Python
environment.

```sh
rho flash -target lolin32-lite examples/c/hello_world/build/output/hello_world.elf
```

The ELF that a build copies to `build/output/` is accepted as well as the
application `.bin` in the generated tree; the build that wrote it is found by
the program's name. `ESP32_PORT` chooses the serial line (default
`/dev/ttyUSB0`), and `ESP32_BAUD` the rate (default 115200):

```sh
ESP32_PORT=/dev/ttyUSB1 rho flash -target lolin32-lite build/output/hello_world.elf
```

The ROM's download handshake runs at 115200 baud, so `ESP32_BAUD` cannot go
below that for the initial sync; esptool uses the lower of the ROM rate and the
requested rate, and a lower value simply stops it hearing the chip at all.

The CH340's DTR/RTS lines normally enter download mode on their own. A board
whose reset circuit does not can be put in download mode by hand — hold
`GPIO0` low, reset the part, then release it — and flashed without esptool
resetting it again:

```sh
ESP32_BEFORE=no-reset rho flash -target lolin32-lite build/output/hello_world.elf
```

If esptool reports that it detected download mode but got no sync reply, the
chip is in the bootloader but its `UART0_RX` (GPIO3) is not receiving: check
the TX line from the USB-serial chip to that pin, the common ground, and the
supply. The boot log arriving means the return path is already working.

## Configuration (Kconfig)

HAL implementations are selected at build time. The platform's own symbols are
declared in `Kconfig`:

| Symbol | Default | Effect |
|---|---|---|
| `RH_HAL_GPIO` | `y` | Compile the GPIO HAL |
| `RH_HAL_I2C` | `y` | Compile the I2C HAL |
| `RH_HAL_SPI` | `y` | Compile the SPI HAL |
| `RH_HAL_UART` | `y` | Compile the UART HAL |
| `RH_HAL_TIMER` | `y` | Compile the Timer HAL |
| `RH_HAL_RS485` | `n` | Compile the RS485 HAL |

A program selects the interfaces it uses in its own configuration
(`project.conf`), alongside the board symbol:

```text
CONFIG_RH_HAL_GPIO=y
CONFIG_RH_HAL_TIMER=y
CONFIG_RH_BOARD_LOLIN32_LITE=y
```

The board adds `RH_CONSOLE_UART_NUM` to choose which UART the console uses;
UART0 is the one the ROM prints to and the one the USB-serial chip is wired to.
A source guarded by a HAL symbol is compiled only when the symbol is set, so a
program that does not enable an interface does not carry its implementation.

## Tests

The host tests are registered by CMake and run under CTest when the project is
built on a host (`RHO_BUILD_HOST_TESTS`, and not a bare-metal target):

```sh
cmake -S . -B build -DRHO_BUILD_HOST_TESTS=ON
ctest --test-dir build
```

A host test exercises the parts of the platform that do not need a peripheral —
the error mapping and the registration the BSP performs — so it runs without a
board attached. Tests that need the silicon are declared rather than run.

## Licence

MIT.

## Maintainer

Aurel Miron — RoboHub Platform Architect, Cluj-Napoca, Romania.

## Release history

[CHANGELOG.md](CHANGELOG.md) — the changes in 0.2.0 and earlier releases.
