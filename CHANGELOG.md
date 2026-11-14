# Changelog

All notable changes to this module are recorded here, one section per release.

## [0.2.0] - 2026-11-01

The LOLIN32 Lite declares the relay/node/edge roles, WiFi and UART/USB links,
its buses and its flash deployment, so a Mesh planner offers it. Its esptool
flash entry writes the bootloader, partition table and application the build
recorded in `flash_args`.

## [0.3.0] - 2026-11-14

The board key is kept in NVS. A production preset turns on secure boot v2, flash encryption and NVS encryption; the eFuse steps are irreversible and run only through `rho flash --production` with confirmation.
