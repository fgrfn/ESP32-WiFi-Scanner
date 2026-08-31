# Troubleshooting

## White or blank display

- Confirm the target is ESP32-2432S028R and the pin map matches the board.
- Confirm `tft_setup.h` is beside the `.ino` and included before `TFT_eSPI.h`.
- Check GPIO 21 backlight control and USB power quality.
- Do not rely on a different global `TFT_eSPI` `User_Setup.h`.

## Inverted colors or wrong RGB/BGR order

The supplied panel requires `ILI9341_2_DRIVER`, `TFT_RGB_ORDER TFT_RGB`, and
`TFT_INVERSION_ON`. A different physical panel revision may need different
settings; change one option at a time and record the board revision.

## Wrong touch direction or alignment

Run **SET > Touch** and press the two targets accurately. If calibration cannot
be used, perform the factory reset and calibrate immediately after restart.
Confirm touch pins 25/39/32/33/36 and `touch.setRotation(0)`.

## `FS was not declared`

Keep `#include <FS.h>` and `using fs::FS;` before `TFT_eSPI.h` and
`WebServer.h`. This matters because `TFT_eSPI` uses `FS_NO_GLOBALS`.

## Multiple libraries found

Enable verbose compile output and remove or rename stale duplicate libraries
from the sketchbook. Confirm the build resolves `TFT_eSPI` `2.5.43` and
`XPT2046_Touchscreen` `1.4`. The local `tft_setup.h` is intentional.

## SD initialization failed

- Insert a FAT-formatted card before enabling logging.
- Check GPIO 18/19/23/5 and card power.
- Try a smaller, known-good card.
- Verify touch still works; a failure after edits often indicates broken SPI bus
  restoration rather than a filesystem problem.

## Memory or partition errors

- Build for **ESP32 Dev Module**, not an ESP32-S2/S3/C3 target.
- Use a partition scheme with sufficient application space (for example the
  default 4 MB flash layout).
- Close duplicate Arduino installations and confirm core `3.3.11`.
- If the sketch exceeds the selected app partition, choose a larger app/no-OTA
  partition and recompile; do not disable safety checks.

## Upload fails

Use a data cable, select the correct port, and try holding BOOT while the upload
begins. Disconnect hardware added to boot-strapping pins while diagnosing.
