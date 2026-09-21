# Contributing to YellowCard

Thank you for helping improve YellowCard. Contributions should preserve the
project's passive, defensive purpose and the constraints of a classic ESP32
with no PSRAM.

## Before You Start

- Read the [README](README.md), [development guide](docs/development.md), and
  [security model](docs/security.md).
- Search existing issues before opening a new one.
- For substantial changes, open an issue first so scope and hardware impact can
  be discussed.
- Do not propose packet injection, deauthentication, credential capture,
  unauthorized association, active BLE interaction, or persistent tracking.

## Fork and Branch Workflow

1. Fork the repository on GitHub.
2. Clone your fork.
3. Create a focused branch from the current default branch:

   ```sh
   git switch -c fix/short-description
   ```

4. Make small, reviewable commits.
5. Push the branch to your fork and open a pull request.

Do not commit directly to a release tag.

## Build Requirement

Every pull request that affects firmware must build successfully with the
public configuration and no private `wifi_secrets.h`:

```sh
pio run -e esp32dev
```

Include the final RAM and flash output in the pull request. Never upload to a
device as part of an automated documentation-only change.

## Keep Secrets Out

Never commit or paste:

- Wi-Fi credentials or private SSIDs.
- API keys, tokens, or `.env` contents.
- Private IP addresses or internal hostnames.
- Private keys or certificates.
- Firmware dumps or binaries containing local credentials.
- Unreviewed serial logs containing nearby wireless identifiers.

Use only placeholder data in examples. Confirm `git status` and `git ls-files`
before submitting. If a secret reaches Git history, stop, rotate it, and report
the incident privately; do not rely on deleting only the latest copy.

## Coding Style

- Follow the existing C++ formatting and naming conventions.
- Prefer small, single-responsibility modules.
- Keep state machines non-blocking so LVGL remains responsive.
- Prefer fixed-size storage and bounded input on the no-PSRAM target.
- Avoid large dynamic allocations and persistent `String` collections.
- Keep UI text in English.
- Keep XPT2046 reads in the LVGL input callback.
- Do not duplicate Wi-Fi or BLE scanners.
- Avoid verbose loop logging; logs must never reveal passwords or payloads.
- Update documentation when behavior, hardware support, or limits change.

## Test Expectations

For firmware changes, report:

- PlatformIO build result.
- RAM and flash use.
- Board variant and display controller.
- Whether testing was build-only or performed on physical hardware.
- Navigation and touch regression results.
- Wi-Fi online/offline behavior where relevant.
- Wi-Fi/BLE scan serialization and heap observations where relevant.

Physical validation is expected for changes involving display, touch, radio
coexistence, scanner lifecycle, timing, or memory. If hardware was unavailable,
state that limitation clearly rather than claiming validation.

## Issues

Use the provided issue forms. A useful report includes reproduction steps,
expected and actual behavior, firmware tag/commit, PlatformIO version, exact
CYD variant, display controller, and sanitized serial logs.

## Pull Requests

Keep each pull request focused. Complete the checklist, explain security and
memory impact, and link related issues. Maintainers may decline features that
expand offensive capability, create privacy risk, or cannot fit safely within
the validated hardware constraints.
