# Development Guide

This document supplements [CONTRIBUTING.md](../CONTRIBUTING.md) with technical
details for firmware changes.

## Stable Baseline

Release v0.4.0 corresponds to the validated F4-B scope:

- Badge, Contacts, and local QR code.
- Multi-Wi-Fi and offline fallback.
- Passive Wi-Fi RF scanner.
- Passive BLE scanner.
- Serialized Wi-Fi/BLE discovery.
- LVGL 8.4.0 on ST7789/XPT2046 hardware.

Changes should preserve a build without `include/wifi_secrets.h`.

## Build

```sh
pio run -e esp32dev
```

The environment pins library versions and uses `huge_app.csv`. Do not add
another environment or change hardware flags without a documented target and
physical validation.

## Coding Guidelines

- Use small modules with clear ownership; avoid unnecessary architecture.
- Keep `loop()` cooperative and avoid blocking waits.
- Prefer fixed-size structures for radio snapshots on this no-PSRAM target.
- Keep network/scanner operations outside LVGL event callbacks; callbacks
  should request work for a state machine.
- Keep XPT2046 reads owned by the LVGL input callback.
- Preserve English UI text.
- Avoid persistent `String` collections and large transient allocations.
- Log state changes and failures, not every loop iteration.
- Never log passwords, raw BLE payloads, or full device addresses.

## Resource Checks

Record PlatformIO RAM and flash values for every firmware pull request. For
changes involving NimBLE, UI allocation, or scan storage, also record observed
runtime free heap and minimum free heap on hardware.

The v0.4.0 reference build is:

```text
RAM:   96,452 / 327,680 bytes (29.4%)
Flash: 1,219,609 / 3,145,728 bytes (38.8%)
```

## Hardware Validation

At minimum, changes affecting firmware should verify:

1. Cold boot and hardware probe.
2. Badge → Contacts → Badge navigation.
3. Badge → RF → Wi-Fi overview/top networks.
4. Badge → RF → BLE overview/top devices.
5. BLE scan while Wi-Fi is online.
6. Offline operation without an available known network.
7. Touch press/release responsiveness after screen changes.
8. Stable free/minimum heap without reboot or panic.

Document the exact board variant and whether validation was build-only or on
physical hardware.

## Debug Flags

Detailed scanner logs are disabled by default:

```ini
-D YELLOWCARD_WIFI_AP_DEBUG=0
-D YELLOWCARD_BLE_DEVICE_DEBUG=0
```

Temporary diagnostics must be rate-limited, sanitized, and removed before a
release unless they provide ongoing operational value.

## Release Checklist

1. Confirm the worktree contains no private configuration or artifacts.
2. Run repository and history secret scans.
3. Build from tracked files without `wifi_secrets.h`.
4. Record RAM/flash and warnings.
5. Perform physical regression testing when firmware changed.
6. Update README, docs, and CHANGELOG.
7. Review tag and release notes before pushing.
8. Never upload firmware or publish a release as an implicit side effect of a
   documentation task.
