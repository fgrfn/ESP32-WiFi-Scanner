# Hardware and pinout

## Supported target

- ESP32-2432S028R / Cheap Yellow Display (classic ESP32)
- 240 × 320 ILI9341 TFT
- XPT2046 resistive touch controller
- On-board active-low RGB LED
- Optional microSD card

## Pin map

| Function | GPIO |
|---|---:|
| TFT MISO | 12 |
| TFT MOSI | 13 |
| TFT SCLK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT backlight | 21 |
| Touch SCLK | 25 |
| Touch MISO | 39 |
| Touch MOSI | 32 |
| Touch CS | 33 |
| Touch IRQ | 36 |
| SD SCLK | 18 |
| SD MISO | 19 |
| SD MOSI | 23 |
| SD CS | 5 |
| RGB red | 4 |
| RGB green | 16 |
| RGB blue | 17 |
| BOOT button | 0 |

## Display configuration

The project-local `tft_setup.h` selects the panel variant:

```cpp
#define ILI9341_2_DRIVER
#define TFT_RGB_ORDER TFT_RGB
#define TFT_INVERSION_ON
```

It also selects HSPI for the TFT and the documented CYD pin map. Different CYD
revisions exist; verify the schematic before changing any pin.

## Touch and SD bus warning

Touch and SD use the same ESP32 `VSPI` controller but different pins. Before an
SD operation, the sketch deselects touch, ends the current bus, and starts VSPI
on the SD pins. It then deselects SD and restores the touch pins. Do not perform
SD and touch transactions concurrently or remove this sequence.
