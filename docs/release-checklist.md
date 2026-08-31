# Release checklist

## Owner decisions

- [ ] Confirm final repository name.
- [ ] Confirm GitHub user or organization.
- [ ] Confirm public or private visibility.
- [ ] Select a license and replace `LICENSE-PENDING.md` with `LICENSE`.
- [ ] Verify the original Arduino code license or obtain written publication
  permission from Marco, including the intended outbound license.
- [ ] Keep the MakerWorld attribution and enclosure link.

## Quality

- [ ] Compile `esp32:esp32:esp32` with ESP32 core `3.3.11`.
- [ ] Record flash and RAM usage and dependency versions.
- [ ] Flash a real ESP32-2432S028R.
- [ ] Verify display colors, orientation, and brightness.
- [ ] Verify first-run and manual touch calibration.
- [ ] Verify first-run language selection before calibration and later language
  changes from both display settings and dashboard.
- [ ] Verify all four brightness levels: 25%, 50%, 75%, and 100%.
- [ ] Verify scans, grouping, pagination, marquee, and pause button.
- [ ] Verify AP tracking, outages, channel changes, and saved state.
- [ ] Verify SD creation and append behavior with touch before/after writes.
- [ ] Verify dashboard, browser time sync, settings, and both CSV exports.
- [ ] Verify factory reset confirmation and restart.
- [ ] Add redacted screenshots to `images/` and update README references.

## Publication

- [ ] Review README links and repository metadata.
- [ ] Run the GitHub Actions build.
- [ ] Create the signed/tagged semantic release and release notes.
- [ ] Do not publish scan logs, SSIDs, BSSIDs, or credentials.
