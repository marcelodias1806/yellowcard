# Passive Wi-Fi RF Scanner

The Wi-Fi RF module visualizes metadata returned by the normal Arduino-ESP32
`WiFi.scanNetworks()` API. It reuses the Wi-Fi manager's asynchronous scan and
does not create a second scanner or enter monitor mode.

## Displayed Information

The RF screens can show:

- Total networks discovered by the latest valid scan.
- Strongest SSID, RSSI, and channel.
- Counts of open and protected networks.
- Occupancy counts for channels 1 through 13, summarized for the screen.
- Up to eight strongest networks with SSID, RSSI, channel, and security label.
- Current ONLINE/OFFLINE connection state.

The snapshot stores at most 16 strongest AP entries. SSIDs are copied into
fixed 33-byte buffers, non-printable bytes are replaced, and the list is sorted
by descending RSSI. Channel and aggregate totals may cover more networks than
the stored top-16 list.

## Scan Lifecycle

- Scans are asynchronous and periodically polled from the main loop.
- A running scan is never replaced by a concurrent scan.
- Results are processed before `WiFi.scanDelete()` releases framework storage.
- Opening Wi-Fi Environment requests a refresh through `WifiManager`.
- If connected, the firmware does not intentionally disconnect the known AP.
- BLE discovery cannot start while the Wi-Fi scanner owns `radio_scan_lock`.

Radio coexistence can affect latency and connection throughput because classic
ESP32 Wi-Fi and BLE share radio resources. Discovery results are snapshots, not
continuous RF measurements.

## Security Boundaries

This module does not provide:

- Packet capture or payload inspection.
- Promiscuous/monitor mode.
- Deauthentication or disassociation.
- Packet injection.
- Credential collection.
- Association with an unknown network.
- Persistence or external transmission of scan data.

Only SSIDs and standard scan metadata broadcast over the air are presented.
Use the feature responsibly and follow applicable laws and venue policies.

## Debug Mode

The default build flag is:

```ini
-D YELLOWCARD_WIFI_AP_DEBUG=0
```

Changing it to `1` enables detailed serial listing of discovered networks.
That can expose nearby SSIDs in logs; keep it disabled for normal public builds
and redact logs before issue submission.
