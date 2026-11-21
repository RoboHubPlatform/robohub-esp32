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

## [0.5.0] - 2026-11-21

The ESP32 takes updates over its link (ESP-0008, ESP-0009, robohub ADR-0024).

- robohub-common's update slot on ESP-IDF's two OTA slots (`src/platform/esp_update.c`): an image is written to the slot not running with sequential writes, so a sector is erased as it is reached; `esp_ota_end` checks it and `esp_ota_set_boot_partition` names it for one boot; the bootloader boots it as `PENDING_VERIFY`. The managed layer confirms it (`esp_ota_mark_app_valid_cancel_rollback`) or rejects it (`esp_ota_mark_app_invalid_rollback_and_reboot`); a hang ends the same way through the watchdog. The update's id and versions, its outcome until taken and the board's security version are one NVS blob, so the old image reports `rolled back` after any reset.
- A development build of the LOLIN32 Lite uses the two-OTA layout with NVS where it was (`config/sdkconfig.ota`: `two_ota_large`, two 1700 KB slots, application rollback confirmed by the application, 4 MB flash). The ordinary flash already writes the bootloader, the table, the initial otadata and the program into `ota_0`, so the next USB flash moves a board; the board says so (`board.flash.ab: true`) for Mesh. A production build keeps its single-app table and stays flash-only.
- The build copies the application image (`<program>.bin`) beside the ELF in the output folder, where an update takes it from.
- The library names ESP-IDF's `app_update` component itself, since it is linked after ESP-IDF's archives and nothing else in a program pulls in the OTA API.
- A host test checks the layout (`tests/ota_layout_test.py`, ESP-0009): the fragment, which builds use it, ESP-IDF's table as ESP-IDF lays it out, and the board's flash. The on-target check of the slot (an update kept, another rolled back, ESP-0008) waits for a board on the bench.

## [0.6.0] - 2026-11-21

The ESP32 on Wi-Fi, and a node template that builds for it (ESP-0010, ESP-0011, robohub ADR-0025).

- `src/platform/esp_net.c` implements robohub-common's network contract:
  - a Wi-Fi station through esp_netif and esp_wifi, brought up once, never erasing NVS (it holds the keystore), and joined by polling with the watchdog fed;
  - TCP and UDP through lwIP's non-blocking BSD sockets, where a receive is a `select` bounded by its timeout and a connect waits at most 5 s.

  The library links ESP-IDF's Wi-Fi, netif, lwIP, event, NVS, timer and FreeRTOS components privately, which brings their include directories. The SMP FreeRTOS kernel's headers are left out of the include globs.
- `robohub_console_write_raw` and `robohub_console_read_byte` reach UART0 through the ROM's console functions, past newlib's `\n` → `\r\n`, so a node's frames and its text share the CH340.
- `component.cmake` links the libraries the program's preset names (`RHO_PROGRAM_LIBRARIES`, rho 0.21.0), so a program gets the node's core, link, telemetry and managed layers.
- The build copies the ELF into the output folder beside the `.bin` when it was linked elsewhere. On a first configure the generated output folder does not reach ESP-IDF's ELF target, and `rho flash`, Mesh and the examples all name `build/output/<program>.elf`.
- `examples/c/health_check`: a watchdog reset, a recorded panic and a fed watchdog, with a key held in NVS across each reset. It is built with the tests, and the run on a board is declared skipped until one is on the bench. `health_check.py` builds it, flashes it and reads the console.
