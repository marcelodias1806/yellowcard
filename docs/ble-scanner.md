# Passive BLE Scanner

YellowCard uses NimBLE-Arduino 2.5.1 in observer-only mode for passive BLE
advertisement discovery. A scan starts only when the BLE screen is opened or
the user taps REFRESH; continuous background scanning is not used.

## Scan Behavior

- Passive scan (`setActiveScan(false)`).
- Five-second scan duration.
- 100 ms interval and 50 ms scan window.
- Maximum 16 devices in the application snapshot.
- Maximum 8 devices displayed, sorted by descending RSSI.
- Device names are sanitized and truncated to 24 characters.
- Unknown devices are labeled `Unknown`.

NimBLE internal result retention is disabled with `setMaxResults(0)`. Discovery
callbacks copy only name, RSSI, name availability, and a volatile address hash
used for deduplication. The strongest repeated RSSI is retained, and a later
name can replace an earlier unknown label.

## Privacy

The scanner does not retain or display full MAC addresses. It does not store
raw advertisement, manufacturer, service, or payload data. The identity hash
and snapshot exist only in RAM for the running firmware session. Results are
not written to flash or sent externally; reboot clears them.

## Lifecycle

The first requested scan initializes NimBLE after verifying Wi-Fi station mode
and `WIFI_PS_MIN_MODEM`. NimBLE remains initialized to avoid unsafe repeated
init/deinit cycles; individual scans are started and stopped as needed. Leaving
the BLE module requests a non-blocking stop if a scan is active and releases
the radio scan lock after completion.

Normal completion reason `0` produces the READY state. Other completion reasons
produce FAILED. Scanner callbacks signal completion, while cleanup and UI state
changes occur through the normal application update path.

## Coexistence

BLE waits while Wi-Fi is scanning or connecting and acquires
`radio_scan_lock` before initialization/scan. The lock is released after
completion, failure, or a completed stop. An existing Wi-Fi connection is not
intentionally disconnected for BLE discovery.

## Security Boundaries

The BLE module does not provide:

- Connections to discovered devices.
- Pairing or bonding.
- GATT service enumeration or reads/writes.
- Active scan requests.
- Spoofing or beacon emulation.
- Packet injection.
- Persistent device tracking.
- External transmission of results.

## Debug Mode

The default flag is:

```ini
-D YELLOWCARD_BLE_DEVICE_DEBUG=0
```

Setting it to `1` logs discovered names and RSSI, but never full addresses or
payloads. Device names can still be sensitive; sanitize logs before sharing.
