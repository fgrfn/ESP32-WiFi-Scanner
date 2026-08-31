# Contributing

Thanks for helping improve WiFi Radar.

## Development setup

1. Install Arduino CLI and ESP32 core `3.3.11`.
2. Install `TFT_eSPI@2.5.43` and `XPT2046_Touchscreen@1.4`.
3. Compile with:

   ```sh
   arduino-cli compile --fqbn esp32:esp32:esp32 WiFi_Radar
   ```

Do not move `FS.h` below `TFT_eSPI.h` or `WebServer.h`. Do not replace the
project-local `tft_setup.h` with a global library edit.

## Pull requests

- Keep changes focused and explain user-visible behavior.
- Run the pinned compile check.
- Test display, touch, scanning, web dashboard, and SD bus switching when the
  change can affect hardware behavior.
- Do not commit credentials, scan exports, MAC addresses, or private SSIDs.
- Update `CHANGELOG.md` for user-visible changes.

## Style

Use two-space indentation in the Arduino sketch, descriptive names, and small
functions. Preserve the asynchronous scan loop and avoid long blocking work.
