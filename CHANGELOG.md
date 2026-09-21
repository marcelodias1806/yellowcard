# Changelog

All notable public changes to YellowCard are documented here.

## v0.4.0

Initial stable public hardware-validated release.

### Added

- Portrait Badge screen with identity and role.
- Contacts screen with LinkedIn and Instagram details.
- Locally generated contact QR code.
- LVGL 8.4.0 foundation with a 20-line partial draw buffer.
- ST7789 display and XPT2046 resistive touch integration.
- Up-to-four-network Wi-Fi configuration with priority and RSSI selection.
- Non-blocking Wi-Fi scan/connect/reconnect state machine.
- Safe offline fallback when credentials or known networks are unavailable.
- Passive Wi-Fi RF overview, channel occupancy, and top-network screens.
- Passive BLE overview and top-device screens using NimBLE-Arduino.
- Fixed-size, RAM-only Wi-Fi and BLE snapshots.
- Wi-Fi/BLE discovery serialization through `radio_scan_lock`.
- ONLINE/OFFLINE status across relevant screens.
- Public-build fallback requiring no private Wi-Fi configuration.

### Changed

- Migrated to the framework-provided `huge_app.csv` partition scheme for a
  larger application partition.
- Configured NimBLE observer-only roles and bounded passive scan lifecycle.
- Applied and verified `WIFI_PS_MIN_MODEM` before NimBLE initialization for the
  validated Wi-Fi/BLE coexistence path.

### Stability

- Corrected asynchronous Wi-Fi scan state handling and result cleanup.
- Prevented concurrent Wi-Fi and BLE discovery scans.
- Kept BLE initialization persistent while starting/stopping scans safely.
- Collected BLE results directly from callbacks when NimBLE internal result
  retention is disabled.
- Restored single-owner XPT2046 access through the LVGL input callback.

### Resource Profile

- Static RAM: 96,452 / 327,680 bytes (29.4%).
- Application flash: 1,219,609 / 3,145,728 bytes (38.8%).
- Firmware binary: 1,226,192 bytes.

Runtime heap figures are documented as observed test values in the README.
