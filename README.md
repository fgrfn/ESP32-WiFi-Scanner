# WiFi Radar

WiFi Radar is a self-contained 2.4 GHz Wi-Fi analyzer for the
ESP32-2432S028R “Cheap Yellow Display” (CYD). It scans nearby networks,
compares access points, tracks signal quality, evaluates channel congestion,
logs CSV data to an optional SD card, and serves a local browser dashboard.

> [!IMPORTANT]
> The classic ESP32 radio sees 2.4 GHz Wi-Fi only. It cannot detect 5 GHz or
> 6 GHz networks and is not a spectrum analyzer.

## Highlights

- Automatic first-boot and manual touch calibration
- Paged network list with scrolling SSIDs
- Optional grouping of identical SSIDs and mesh access points
- BSSID/AP comparison with offline vendor identification
- RSSI, signal quality, channel, and security information
- Signal history with minimum, maximum, average, outages, and channel changes
- Weighted channel analysis with recommendations for channels 1, 6, and 11
- Security overview and open-network warning
- Persistent NVS settings, brightness, and light/dark themes
- RGB status LED and optional SD CSV logging
- Local responsive dashboard with charts, settings, and CSV export
- Browser-based time synchronization for real timestamps
- Confirmation-protected factory reset

## Screenshots

No photos were included in the supplied project archive. Hardware and UI
screenshots can be added to `images/` before release; remove private SSIDs,
BSSIDs, IP addresses, and other identifying data first.

## Hardware

The target is the classic ESP32-based ESP32-2432S028R/CYD with a 240 × 320
ILI9341 TFT and XPT2046 touch controller. See [hardware details](docs/hardware.md)
for the complete pin map and shared-bus warning.

## Quick start

1. Install Arduino IDE and the Espressif ESP32 board package `3.3.11`.
2. Install `TFT_eSPI` `2.5.43` and `XPT2046_Touchscreen` `1.4`.
3. Open `WiFi_Radar/WiFi_Radar.ino`.
4. Select **ESP32 Dev Module** (`esp32:esp32:esp32`).
5. Connect the CYD, select its port, then verify and upload.
6. On first boot, touch the upper-left and lower-right calibration targets.

The display setup is project-local. `build_opt.h` preloads it for the sketch and
the separately compiled `TFT_eSPI` source; do not edit the installed library.
Full instructions are in
[Installation](docs/installation.md).

## Device controls

- Tap a network for signal details and history.
- Tap **AP-LISTE** to compare BSSIDs advertising the selected SSID.
- Tap **LIST** repeatedly or swipe horizontally to change result pages.
- Tap **KANAL**, then **SICHER**, for channel and security views.
- Tap **SET** for scan, display, SD, web, calibration, and reset settings.
- Press the BOOT button or tap **PAUSE** to pause/resume scanning.

See the [complete UI guide](docs/display-ui.md).

## Web dashboard

Enable **Web-Dashboard** under **SET** and connect a phone or computer to:

| Setting | Value |
|---|---|
| SSID | `WiFi-Radar-V2` |
| Password | `radar1234` |
| URL | `http://192.168.4.1` |

This access point provides no internet connection. Opening the dashboard sends
the browser time to the ESP32. Because the board has no real-time clock, repeat
this after every reboot to obtain real timestamps. See the
[dashboard guide](docs/web-dashboard.md).

> [!WARNING]
> The default password is public. Use the dashboard only in a trusted physical
> environment, or change it in the sketch before deployment.

## SD logging

When enabled, the device appends scan rows to `/wifi-radar.csv` and tracked
network samples to `/wifi-history.csv`. Without browser time synchronization,
timestamps use `U+seconds` since boot. Formats and bus behavior are documented
in [SD logging](docs/sd-logging.md).

## RGB LED

| Color | State |
|---|---|
| Blue | Scan running |
| Green | Scan complete; no open network found |
| Yellow | At least one open network found |
| Red | Scanning paused |

The CYD RGB LED is active-low.

## Build reproducibility

CI pins ESP32 core `3.3.11`, `TFT_eSPI@2.5.43`, and
`XPT2046_Touchscreen@1.4`, then compiles for `esp32:esp32:esp32`. The workflow
is in `.github/workflows/compile.yml`.

## Known limitations

- 2.4 GHz Wi-Fi only; no 5/6 GHz support.
- No RTC. Browser time sync is lost after reboot.
- Channel recommendations model Wi-Fi overlap and RSSI, not non-Wi-Fi noise.
- The scan stores the 30 strongest networks.
- Vendor detection uses a small offline OUI table and can return “Unbekannt”.
- Touch and SD share one ESP32 SPI controller on different pins; the controlled
  bus switch in the sketch is hardware-critical.

## Documentation

- [Hardware and pinout](docs/hardware.md)
- [Installation and flashing](docs/installation.md)
- [Display UI](docs/display-ui.md)
- [Web dashboard](docs/web-dashboard.md)
- [SD logging and CSV formats](docs/sd-logging.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Release checklist](docs/release-checklist.md)
- [Verified build report](BUILD_REPORT.md)
- [License options](docs/licensing.md)
- [Deutsche README](README.de.md)

## License

License selection is intentionally pending. No GitHub publication should occur
until the owner confirms the final repository name, owner/organization,
visibility, and license. Because this firmware began from code supplied with a
MakerWorld project, the original code's redistribution terms or the creator's
permission must also be confirmed. See [Attribution](ATTRIBUTION.md) and
[license options](docs/licensing.md).

## Acknowledgements

The original idea and initial code basis came from Marco's
[WIFI ANALIZER - ESP32 LVGL 2432S028 2.8\"](https://makerworld.com/de/models/2303837-wifi-analizer-esp32-lvgl-2432s028-2-8#profileId-2514600).
That project also offers a matching printable enclosure. This repository does
not redistribute the enclosure files; obtain them from the original page and
observe its license. The full provenance note is in [ATTRIBUTION.md](ATTRIBUTION.md).
