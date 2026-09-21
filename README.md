# YellowCard

**ESP32 Cybersecurity Interactive Badge**

YellowCard is a compact interactive badge built around an ESP32 display board.
The current firmware provides a touch-enabled identity card, a locally
generated QR contact page, optional known-network Wi-Fi, and an offline mode.

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
- Non-blocking selection of up to four known Wi-Fi networks
- Fully functional offline mode when no known network is available
- Passive Wi-Fi RF and BLE discovery dashboards

## Wi-Fi selection policy

Known networks have an explicit numeric priority in the local
`wifi_secrets.h`; a higher value means higher priority. After an asynchronous
scan, YellowCard selects the available known network with the highest priority.
RSSI is the secondary criterion when two configured networks have the same
priority. Connection attempts time out without blocking LVGL, and failed or
lost connections return to offline mode before a periodic rescan.

Wi-Fi persistence is disabled, so credentials are not written to NVS by this
firmware. With no local `wifi_secrets.h`, the safe zero-network fallback is
compiled and the Wi-Fi radio is not started.

## Build

Install PlatformIO, open the project directory, and run:

```sh
pio run -e esp32dev
```

This command builds the firmware without uploading it. Device upload should be
performed explicitly only after reviewing the target port, environment, and
firmware.

## Local configuration and secrets

Real credentials and private endpoints must never be committed. Templates are
provided at:

- `include/wifi_secrets.example.h`
- `include/api_config.example.h`

For local development, copy a template to its filename without
`.example` and edit only the local copy. `include/wifi_secrets.h` and
`include/api_config.h` are ignored by Git.

The original firmware backup under `backup/`, PlatformIO artifacts, virtual
environments, editor state, firmware binaries, keys, and common secret files
are also excluded from version control.

## Roadmap

- Tecnocorp Lab Monitor
- Presentation Mode

Any future RF scanning feature is intended strictly for passive discovery. It
will not perform deauthentication, exploitation, credential capture, or other
active interference.

## License

Licensed under the MIT License. See [LICENSE](LICENSE).
