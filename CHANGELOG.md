# Changelog

All notable changes to this project will be documented in this file. The format
is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project intends to use [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- English and German project documentation.
- Reproducible Arduino CLI build workflow for ESP32 core 3.3.11.
- Bug report, feature request, pull request, and release templates.
- Project-origin attribution and a link to Marco's original MakerWorld project
  and matching enclosure.

### Changed

- Explicitly load the project-local TFT configuration before `TFT_eSPI.h`.
- Preserve the ESP32 core 3.3.11-compatible `FS.h` include order.

## [2.1.0] - 2026-08-31

### Added

- Touch calibration, paged WLAN list, SSID grouping, AP comparison, signal
  history, channel and security analysis, OUI lookup, settings persistence,
  RGB status LED, SD CSV logging, and local web dashboard.
