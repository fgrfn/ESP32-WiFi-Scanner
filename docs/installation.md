# Installation

## Requirements

- Arduino IDE 2.x or Arduino CLI
- Espressif ESP32 Arduino core `3.3.11`
- `TFT_eSPI` `2.5.43`
- `XPT2046_Touchscreen` `1.4`
- Data-capable USB cable and the board's correct USB-to-serial driver

`FS`, `SPI`, `SD`, `WiFi`, `Preferences`, and `WebServer` are supplied by the
ESP32 core.

## Arduino IDE

1. Add Espressif's board manager URL in Arduino IDE preferences if necessary.
2. Open Boards Manager, install **esp32 by Espressif Systems** version `3.3.11`.
3. Open Library Manager and install the two pinned libraries above.
4. Open `WiFi_Radar/WiFi_Radar.ino`.
5. Select **ESP32 Dev Module**. The equivalent FQBN is
   `esp32:esp32:esp32`.
6. Select the serial port.
7. Click **Verify**, then **Upload**.

If upload cannot connect, hold BOOT, start upload, and release BOOT when the IDE
shows that it is connecting.

## Arduino CLI

```sh
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.11
arduino-cli lib install TFT_eSPI@2.5.43
arduino-cli lib install XPT2046_Touchscreen@1.4
arduino-cli compile --fqbn esp32:esp32:esp32 WiFi_Radar
```

## Critical include order

ESP32 core 3.3.11 requires the sketch to preserve this order:

```cpp
#include <FS.h>
using fs::FS;
#include <SPI.h>
#include <SD.h>
#include "tft_setup.h"
#include <TFT_eSPI.h>
// ...
#include <WebServer.h>
```

`build_opt.h` preloads `tft_setup.h` for every compilation unit, including the
separately compiled `TFT_eSPI.cpp`. Keep all three files together; including the
setup only in the `.ino` is not sufficient for a reproducible library build.

`TFT_eSPI` defines `FS_NO_GLOBALS`; moving `FS.h` below it can make
`WebServer.h` fail with `FS was not declared`. The local setup header must also
appear before `TFT_eSPI.h`.

## First boot

Select Deutsch or English on the initial screen. Then touch the target at the
upper left, release, and touch the target at the lower right. Language and
calibration are stored in NVS. Scanning begins after both touches succeed.
