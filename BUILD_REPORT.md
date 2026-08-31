# Verified build report

Build verified locally on 2026-08-31.

## Result

| Item | Result |
|---|---|
| Status | Successful, exit code 0 |
| FQBN | `esp32:esp32:esp32` |
| ESP32 core | `3.3.11` |
| Arduino CLI | `1.5.1` |
| TFT_eSPI | `2.5.43` |
| XPT2046_Touchscreen | `1.4` (Library Manager release 1.4.0) |
| Program storage | 1,069,660 / 1,310,720 bytes (81%) |
| Global/dynamic memory | 51,812 / 327,680 bytes (15%) |
| Remaining local-variable memory | 275,868 bytes |
| Compiler warnings | None with `--warnings all` |

Command shape:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all WiFi_Radar
```

The clean verification used a new build directory, so `TFT_eSPI.cpp` was
recompiled with the project-local `build_opt.h` and `tft_setup.h`; it did not
reuse a globally configured library object.

## Source changes from the supplied V2.1 archive

- Renamed the Arduino sketch/folder pair to `WiFi_Radar/WiFi_Radar.ino` for a
  clean repository layout; behavior remains V2.1.
- Added an explicit `#include "tft_setup.h"` before `TFT_eSPI.h`.
- Added `build_opt.h` to preload the local TFT setup for separately compiled
  library sources.
- Defined the existing touch CS GPIO 33 in the shared local setup.
- Added explicit RSSI and channel casts to eliminate type/format warnings; no
  values or runtime decisions changed.
- Added persistent German/English selection before first-run touch calibration,
  later language switching in display settings, and matching dashboard localization.
- Restricted brightness to the persistent 25%, 50%, 75%, and 100% levels on
  both the display and dashboard; legacy values are normalized on load.
- Preserved `FS.h`/`using fs::FS` before `TFT_eSPI` and `WebServer`.
- Preserved controlled switching of the shared touch/SD SPI controller.

## Hardware validation still required

- Flash and boot on the exact ESP32-2432S028R panel revision.
- Confirm display colors, inversion, orientation, and backlight range.
- Confirm first-run/manual touch calibration and mapping.
- Confirm first-run language selection works before calibration and that later
  display/dashboard language changes persist across restart.
- Confirm the four brightness levels on the physical backlight.
- Exercise scanning, mesh grouping, AP tracking, pause button, and RGB LED.
- Test SD initialization, sustained CSV writes, and touch restoration.
- Test dashboard, time sync, downloads, settings, and factory reset.

## Remaining risks

- CYD boards have multiple panel and wiring revisions despite similar product
  names; the supplied pinout must match the physical board.
- At 81% of the default OTA application partition, future feature growth has
  limited headroom unless code size is reduced or the partition scheme changes.
- The dashboard has no application-level authentication and uses a documented
  default access-point password.
- No screenshots were available in the supplied archive.
