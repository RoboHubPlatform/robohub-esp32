---
id: ADR-0002
title: Production is secure boot v2, release-mode flash encryption and NVS encryption together
status: proposed
date: 2026-11-14
requirements: [ESP-0006, ESP-PROD-0001, ESP-PROD-0002, ESP-PROD-0003]
supersedes: []
superseded_by: []
---

# Production is secure boot v2, release-mode flash encryption and NVS encryption together

## Context

The device key lives in NVS (ADR-0001), which is plain text on the flash. NVS
encryption protects it but requires flash encryption, and flash encryption
without secure boot lets anyone flash an image that reads the key back. On the
ESP32 these features are switched on by eFuses, which the second-stage
bootloader burns the first time a production image boots — not by the flasher.

## Decision

One production preset, applied only with `CONFIG_RH_PRODUCTION=y`, turns on all
three: secure boot v2 (RSA-3072, chip revision 3.0 or later), flash encryption
in release mode and NVS encryption, with the partition table moved to `0x10000`
and an encrypted `nvs_keys` partition. The preset gets its own sdkconfig,
rewritten at every configure, and the signing key is named only by
`RH_SECURE_BOOT_SIGNING_KEY`. `rho flash --production` follows
`production/esp32.json`: it writes the signed bootloader without resetting the
board, then the image, and lists the eFuses the first boot will burn before it
asks for the board's name.

## Consequences

- A board that has booted a production image accepts only images signed with
  the same key, and plaintext serial flashing stops working; updates must come
  over the air or as encrypted images.
- Losing the signing key leaves the board unable to take a new image.
- Release mode cannot be relaxed to development mode afterwards.
