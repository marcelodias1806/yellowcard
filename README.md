# YellowCard

**ESP32 Cybersecurity Interactive Badge**

YellowCard is a compact interactive badge built around an ESP32 display board.
The current firmware provides a touch-enabled identity card and a locally
generated QR contact page, with no network connectivity enabled.

## Hardware

- ESP32-2432S028 2USB
- ESP32-D0WD-V3 revision 3
- ST7789 240×320 display
- XPT2046 resistive touchscreen
- 4 MB flash
- No PSRAM

## Stack

- PlatformIO
- Arduino Framework
- LVGL 8.4.0
- TFT_eSPI
- XPT2046_Touchscreen

## Current features

- Portrait badge screen
- Locally generated QR contact page
- Touch user interface
- Partial LVGL draw buffer suitable for operation without PSRAM

## Build

Install PlatformIO, open the project directory, and run:

```sh
pio run
```

The command builds the firmware without uploading it. Device upload should be
performed explicitly only after reviewing the target port and firmware.

## Local configuration and secrets

Real credentials and private endpoints must never be committed. Templates are
provided at:

- `include/wifi_secrets.example.h`
- `include/api_config.example.h`

For future local development, copy a template to its filename without
`.example` and edit only the local copy. `include/wifi_secrets.h` and
`include/api_config.h` are ignored by Git. Neither file is required by the
current offline firmware.

The original firmware backup under `backup/`, PlatformIO artifacts, virtual
environments, editor state, firmware binaries, keys, and common secret files
are also excluded from version control.

## Roadmap

- Multi-Wi-Fi
- Offline Mode
- Wi-Fi RF Scanner
- BLE Scanner
- IPOnline Monitor
- Tecnocorp Lab Monitor
- Presentation Mode

Any future RF scanning feature is intended strictly for passive discovery. It
will not perform deauthentication, exploitation, credential capture, or other
active interference.

## License

Licensed under the MIT License. See [LICENSE](LICENSE).
