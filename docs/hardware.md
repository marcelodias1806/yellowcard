# Hardware

## Validated Board

YellowCard v0.4.0 was developed and physically validated on a 2-USB style
ESP32 Cheap Yellow Display / ESP32-2432S028 variant with:

| Item | Value |
| --- | --- |
| MCU | ESP32-D0WD-V3 |
| CPU | Dual-core, up to 240 MHz |
| Flash | 4 MB |
| PSRAM | None |
| Display controller | ST7789 |
| Display geometry | 240×320 portrait |
| Touch controller | XPT2046 resistive |
| USB serial | CH340 |
| Radio | 2.4 GHz Wi-Fi and Bluetooth/BLE |

## Validated hardware

The firmware documented in this repository was physically validated on the
ESP32-2432S028 board shown below.

### Front

![YellowCard hardware front](images/yellowcard-hardware-front.jpg)

### Back / PCB

![YellowCard hardware back](images/yellowcard-hardware-back.jpg)

The tested board includes:

- ESP32-WROOM-32 module
- 2.8-inch 240×320 resistive touchscreen
- ST7789 display controller configuration used by this project
- XPT2046-compatible resistive touch controller
- 4 MB flash
- No PSRAM
- USB-C and Micro-USB connectors
- microSD slot
- onboard RGB LED
- speaker connector
- external expansion connectors

> Cheap Yellow Display boards exist in multiple hardware revisions.
> Visually compare your PCB with the board above before assuming the
> display and touchscreen configuration is identical.

## Variant Warning

“ESP32-2432S028” and “Cheap Yellow Display” identify a family, not one
guaranteed schematic. Some boards use ILI9341 displays, different USB bridges,
different backlight behavior, or different touch wiring. This repository's
build flags are specifically for the validated **ST7789 / 2-USB style** board.

Before uploading, compare the controller markings, seller schematic, and board
revision. A blank screen, incorrect geometry, or wrong colors can indicate a
different controller rather than a firmware defect.

## Display Pinout

| ST7789 signal | ESP32 GPIO | Notes |
| --- | ---: | --- |
| MISO | 12 | Display SPI input path |
| MOSI | 13 | Display SPI output |
| SCLK | 14 | Display clock |
| CS | 15 | Display chip select |
| DC | 2 | Data/command |
| RST | -1 | No dedicated GPIO configured |
| BL | 21 | Backlight, active HIGH |

The display uses BGR color order and a 40 MHz SPI clock in the validated
configuration.

## Touch Pinout

| XPT2046 signal | ESP32 GPIO | Notes |
| --- | ---: | --- |
| CLK | 25 | Dedicated touch SPI clock |
| MOSI | 32 | Dedicated touch SPI output |
| MISO | 39 | Input-only GPIO, touch SPI input |
| CS | 33 | Touch chip select |
| IRQ | 36 | Input-only GPIO, active-low interrupt |

The touch controller uses a dedicated `SPIClass(HSPI)` and is not handled by
TFT_eSPI. The validated orientation is portrait rotation 0. Calibration values
are centralized in `src/lvgl_port.cpp`; do not copy them blindly to a different
panel or board revision.

## Display and Touch Buses

The display and touch controllers use different pin sets. TFT_eSPI owns the
display bus configured through PlatformIO build flags. `XPT2046_Touchscreen`
owns the dedicated touch bus initialized in `lvgl_port`.

This separation explains the expected TFT_eSPI compile warning that
`TOUCH_CS` is not defined. Defining TFT_eSPI touch support is unnecessary and
could create a second owner for a controller already managed independently.

## Power and USB

Use a stable USB source and a data-capable cable. The CH340 bridge normally
appears as `/dev/ttyUSB*` on Linux/WSL and as a COM port on Windows. Some CYD
boards expose two USB connectors with different power/serial roles; verify the
labels and board documentation.

## Resource Constraints

The validated device has no PSRAM. YellowCard therefore uses fixed-size scan
snapshots and a 20-line partial LVGL draw buffer. The `huge_app.csv` partition
scheme increases application space but removes OTA slot redundancy.
