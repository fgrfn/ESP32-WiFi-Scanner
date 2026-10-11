# Changelog

All notable changes to this project will be documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project intends to use [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Fixed

- Escaped control characters and HTML metacharacters in scanned network names
  before rendering them in the local web dashboard.

## [2.2.0] - 2026-09-01

### Added

- New ESP32 WiFi Scanner branding and transparent repository logo.
- First-boot German/English selection before touch calibration.
- Persistent language switching from display settings and the web dashboard.
- English and German project documentation.
- Reproducible Arduino CLI build workflow for ESP32 core 3.3.11.
- Bug report, feature request, pull request, and release templates.
- Project-origin attribution and a link to Marco's original MakerWorld project
  and matching enclosure.

### Changed

- Renamed the sketch, dashboard, access point, scan CSV, and documentation from
  WiFi Radar to ESP32 WiFi Scanner.
- Consolidated repository-facing documentation in English.
- Replaced continuous/arbitrary brightness values with 25%, 50%, 75%, and 100%.
- Localized the display UI and web dashboard in German and English.
- Explicitly load the project-local TFT configuration before `TFT_eSPI.h`.
- Preserve the ESP32 core 3.3.11-compatible `FS.h` include order.

## [2.1.0] - 2026-08-31

### Added

- Touch calibration, paged WLAN list, SSID grouping, AP comparison, signal
  history, channel and security analysis, OUI lookup, settings persistence,
  RGB status LED, SD CSV logging, and local web dashboard.
