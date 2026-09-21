## Summary

Describe the focused change and why it is needed.

## Related issue

Closes #

## Validation

- PlatformIO command:
- Build result:
- Static RAM:
- Application flash:
- Board variant:
- Display controller:
- Validation type: build-only / physical hardware
- Physical test results, if applicable:

## Security and privacy impact

Describe any change to radio behavior, collected/displayed data, credential
handling, persistence, or external communication. Write “None” when applicable.

## Checklist

- [ ] `pio run -e esp32dev` succeeds without private `wifi_secrets.h`.
- [ ] I did not commit passwords, private SSIDs, tokens, private IPs, keys, certificates, or unreviewed logs.
- [ ] The change preserves passive discovery and adds no injection, deauthentication, credential capture, unauthorized association, active BLE access, spoofing, or tracking.
- [ ] LVGL/main-loop behavior remains non-blocking.
- [ ] I recorded RAM and flash impact for firmware changes.
- [ ] I documented build-only versus physical-hardware validation accurately.
- [ ] I updated user/developer documentation where needed.
- [ ] I did not include generated build artifacts or firmware dumps.
