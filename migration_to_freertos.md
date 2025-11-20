# RayZ ESP32 Migration Notes

## Overview

This document captures the troubleshooting steps, fixes, and lessons learned while migrating the RayZ firmware from the Arduino framework to the ESP-IDF (FreeRTOS-based) toolchain. It follows the chronology from the original `package.json` manifest failure to the latest stability fixes and highlights areas where we can still improve.

---

## Chronological Problem Log

### 1. `MissingPackageManifestError`

- **Symptom:** PlatformIO aborted early with a `MissingPackageManifestError` when trying to build the project.
- **Root Cause:** The build still referenced the legacy Arduino-style library structure. PlatformIO expected an ESP-IDF component manifest (CMake) instead of `library.json`/`package.json`.
- **Fix:**
  - Recreated the shared code as a proper ESP-IDF component by adding a `CMakeLists.txt` and declaring `REQUIRES` dependencies (`nvs_flash`, `bt`, etc.).
  - Registered the shared component directory via `board_build.extra_component_dirs` inside each project `platformio.ini`.
  - Removed the stale Arduino metadata files so the ESP-IDF build system could index the sources correctly.

### 2. Shared Component Resolution

- **Symptom:** After the CMake conversion PlatformIO still linked against an outdated global `rayz-shared` library copy located in the user profile.
- **Root Cause:** PlatformIO automatically loads globally installed libraries with matching names before local components.
- **Fix:**
  - Added `lib_ignore = rayz-shared` to both `target/platformio.ini` and `weapon/platformio.ini` to force PlatformIO to use the in-repo component.
  - Validated the change by rerunning `pio run`, confirming the correct sources compiled (no more legacy build warnings).

### 3. Partition Table Alignment

- **Symptom:** `Partition ota_0 invalid: Offset 0x11000 is not aligned to 0x10000` during `partitions.bin` generation.
- **Root Cause:** OTA slot offsets inherited from the Arduino partition layout were not on 64 KB boundaries, which ESP-IDF requires when using OTA-capable bootloaders.
- **Fix:**
  - Re-authored `shared/partitions/huge_app.csv` so each OTA image starts on a `0x10000` boundary.
  - Adjusted OTA slot sizes and the SPIFFS region to stay within the 4 MB flash map.
  - Rebuilt successfully for both target (ESP32) and weapon (ESP32-C3) applications.

### 4. NimBLE / FreeRTOS API Migration

- **Symptom:** Compilation errors in the shared BLE layer after switching frameworks (callback signatures, FreeRTOS includes, etc.).
- **Root Cause:** Arduino wrapped ESP-IDF with legacy headers; raw ESP-IDF 5.5 introduced updated NimBLE APIs, stricter include ordering, and new FreeRTOS expectations.
- **Fix:**
  - Audited each callback and updated prototypes to match NimBLE 1.4+ signatures (e.g., discovery handlers, GATT descriptors, write callbacks).
  - Normalised FreeRTOS include order (`FreeRTOS.h` first) and moved ISR-related code into component sources.
  - Added helper utilities (`parse_uuid128`, dynamic notification registration) to keep logic consistent across both projects.

### 5. Build Configuration Drift

- **Symptom:** Inconsistent flash size warnings (`Expected 4MB, found 2MB`) and duplicated sdkconfig entries between projects.
- **Root Cause:** The Arduino build system masked board definitions; ESP-IDF surfaces mismatches immediately.
- **Fix:**
  - Consolidated configuration in `sdkconfig.defaults` and re-ran `pio run -t menuconfig` for each environment to regenerate consistent settings.
  - Documented the warning so the team can adjust the flash size if hardware variations are detected.

---

## Detailed Technical Changes

1. **Directory Layout**

   - Shared code now lives under `shared/src` and `shared/include` with a dedicated `CMakeLists.txt`.
   - `CMakeLists.txt` exports the component via `idf_component_register` and declares its dependencies.
   - Both PlatformIO projects point to `../shared` using `board_build.extra_component_dirs`.

2. **Build System Adjustments**

   - Added `lib_ignore` directives to ensure the in-repo component takes precedence over any globally installed library with the same name.
   - Updated `partition.csv` to the ESP-IDF format with correct alignments and sizes for OTA deployments.
   - Switched firmware entry-points to FreeRTOS tasks (`app_main`, task creation) instead of Arduino `setup()/loop()`.

3. **BLE/NimBLE Refactor**

   - Replaced Arduino BLE wrappers with NimBLE API calls available in ESP-IDF 5.5.
   - Implemented dedicated helper methods for UUID parsing, service discovery, and notification subscription consistent with NimBLE expectations.
   - Ensured thread-safe communication with FreeRTOS primitives (queues, event groups) explicitly included via ESP-IDF headers.

4. **Configuration Consistency**
   - `sdkconfig.defaults` now holds any project-wide configuration (flash size, Wi-Fi/BLE options, logging level).
   - PlatformIO environments set `board_build.partitions = ../shared/partitions/huge_app.csv` to share the same memory map.
   - Confirmed builds against `espressif32@6.12.0` with `framework-espidf 5.5.0` compile cleanly.

---

## Lessons Learned

- **Component-first mindset:** ESP-IDF expects each logical unit to be a component. Aligning the structure early avoids manifest errors.
- **Global library hygiene:** PlatformIO caches libraries globally. When reorganising, immediately add `lib_ignore` or rename components to prevent silent conflicts.
- **Partition planning matters:** OTA-capable systems demand 64 KB alignment; double-check CSV layouts after any migration.
- **APIs change across frameworks:** Moving away from Arduino surfaces breaking changes in NimBLE and FreeRTOS headers; allocate time for a full audit, not just mechanical migration.

---

## Future Improvements

1. **Automated Consistency Checks**

   - Add CI steps that build both target and weapon environments to catch configuration drift sooner.
   - Include scripts that verify partition alignment and flash size assumptions.

2. **Documentation Automation**

   - Generate component diagrams (CMake graph or dependency graph) to quickly identify cross-project impacts.

3. **Testing Enhancements**

   - Port Arduino-era unit tests to ESP-IDF Unity or GoogleTest, ensuring both platforms exercise shared BLE logic.
   - Add integration tests for OTA updates to validate the new partitioning strategy on hardware.

4. **Configuration Sync Tools**

   - Introduce a common `sdkconfig.defaults` generator to keep target/weapon configurations aligned when toggling features.

5. **BLE Abstraction Layer**
   - Consider wrapping NimBLE interactions in a stricter interface so future ESP-IDF upgrades require changes in one place only.

---

## Appendix: Quick Reference Commands

```bash
# Build target firmware
pio run -d target

# Build weapon firmware
pio run -d weapon

# Upload both firmwares (requires USB connections)
pio run -d target -t upload
pio run -d weapon -t upload

# Regenerate sdkconfig with menuconfig dialog
pio run -d target -t menuconfig
pio run -d weapon -t menuconfig
```
