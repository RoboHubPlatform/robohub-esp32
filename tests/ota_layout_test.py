#!/usr/bin/env python3
"""The ESP32's two-slot layout, checked on the host (ESP-0009, robohub ADR-0024).

- A development build's configuration fragment (config/sdkconfig.ota) picks
  ESP-IDF's two-OTA table, application rollback confirmed by the application and
  4 MB of flash; the board's idf.cmake uses it for every build that is not
  production, and a production build keeps its own table.
- ESP-IDF's table, laid out as ESP-IDF lays it out, keeps NVS where the
  single-app table had it (0x9000, 24 kB), puts `ota_0` at 0x20000 — where the
  ordinary flash writes the program — and fits both slots in 4 MB.
- The board's flash writes `flash_args` (bootloader, table, initial otadata,
  program) and says it leaves two slots (`board.flash.ab: true`); the build
  copies the application image beside the ELF, where an update takes it from.

@project RoboHub Platform
@author Aurel Miron
@date 2026

SPDX-License-Identifier: MIT
"""

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARD = ROOT / "boards" / "lolin32-lite"
FLASH_SIZE = 4 * 1024 * 1024
TABLE_END = 0x9000  # the partition table at 0x8000, one sector

failures = []


def check(ok, what):
    print(("[PASS] " if ok else "[FAIL] ") + what)
    if not ok:
        failures.append(what)


def size_of(text):
    text = text.strip()
    if text.lower().endswith("k"):
        return int(text[:-1], 0) * 1024
    if text.lower().endswith("m"):
        return int(text[:-1], 0) * 1024 * 1024
    return int(text, 0)


def layout(csv):
    """Offsets as ESP-IDF's gen_esp32part assigns them when none is given:
    data partitions on 4 kB, app partitions on 64 kB."""
    out = {}
    offset = TABLE_END
    for line in csv.splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        name, kind, subtype, at, size = [f.strip() for f in line.split(",")[:5]]
        align = 0x10000 if kind == "app" else 0x1000
        start = int(at, 0) if at else (offset + align - 1) // align * align
        out[name] = (start, size_of(size), kind, subtype)
        offset = start + size_of(size)
    return out


def idf_path():
    for candidate in (os.environ.get("IDF_PATH", ""), str(ROOT / "vendor" / "esp-idf")):
        if candidate and (Path(candidate) / "components" / "partition_table").is_dir():
            return Path(candidate)
    return None


# --- The configuration fragment ---------------------------------------------
fragment = (ROOT / "config" / "sdkconfig.ota").read_text()
values = dict(re.findall(r"^(CONFIG_[A-Z0-9_]+)=(.*)$", fragment, re.M))
check(values.get("CONFIG_PARTITION_TABLE_TWO_OTA_LARGE") == "y", "the two-OTA table (two_ota_large)")
check(values.get("CONFIG_BOOTLOADER_APP_ROLLBACK") == "y", "application rollback")
check(values.get("CONFIG_BOOTLOADER_APP_ROLLBACK_CONFIRM_BY_APP") == "y",
      "an image is confirmed by the application, not at startup")
check(values.get("CONFIG_ESPTOOLPY_FLASHSIZE_4MB") == "y", "the board's 4 MB of flash")

# --- Which builds use it ----------------------------------------------------
idf_cmake = (BOARD / "idf.cmake").read_text()
production_at = idf_cmake.find("partitions.production.csv")
ota_at = idf_cmake.find("config/sdkconfig.ota")
else_at = idf_cmake.find("else()", production_at)
check(production_at >= 0, "a production build keeps its own table")
check(ota_at > else_at > production_at, "the two-slot fragment is used by every build that is not production")
check("${CMAKE_PROJECT_NAME}.bin" in idf_cmake and "output" in idf_cmake,
      "the build copies the application image into the output folder")

# --- ESP-IDF's tables -------------------------------------------------------
idf = idf_path()
if idf is None:
    print("[SKIP] ESP-IDF's partition tables: no ESP-IDF (IDF_PATH or vendor/esp-idf)")
else:
    tables = idf / "components" / "partition_table"
    two = layout((tables / "partitions_two_ota_large.csv").read_text())
    single = layout((tables / "partitions_singleapp.csv").read_text())
    check(two.get("nvs", (0, 0))[:2] == (0x9000, 0x6000), "NVS at 0x9000, 24 kB")
    check(two.get("nvs", (0, 0))[:2] == single.get("nvs", (1, 1))[:2], "NVS where the single-app table had it")
    check("otadata" in two, "an otadata partition for the bootloader's choice")
    check(two.get("ota_0", (0,))[0] == 0x20000, "ota_0 at 0x20000, where the ordinary flash writes the program")
    check(two.get("ota_0", (0, 0))[1] == two.get("ota_1", (0, 1))[1] == 1700 * 1024, "two equal 1700 kB slots")
    end = max(start + size for start, size, _, _ in two.values())
    check(end <= FLASH_SIZE, f"the layout fits 4 MB (ends at {end:#x})")

# --- The board's flash ------------------------------------------------------
manifest = (BOARD / "module.yml").read_text()
flash = manifest[manifest.find("\n  flash:") + 1:]
following = re.search(r"\n  [a-z_]+:", flash)  # the next key beside `flash`
if following:
    flash = flash[:following.start()]
check("@flash_args" in flash, "the flash writes what the build recorded in flash_args")
check(re.search(r"^\s+ab:\s*true\s*$", flash, re.M) is not None,
      "the board says its flash leaves two slots (board.flash.ab), which Mesh records")

if failures:
    print(f"{len(failures)} failed")
    sys.exit(1)
print("ota layout: passed")
