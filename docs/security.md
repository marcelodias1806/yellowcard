# Security and Privacy Design

YellowCard is an educational and defensive visualization project. Its RF
features are intentionally narrower than general wireless assessment tools.

## Threat Boundaries

The firmware performs:

- Association only with locally configured known Wi-Fi networks.
- Standard asynchronous Wi-Fi network discovery.
- Passive BLE advertisement discovery.
- Local display of sanitized, bounded snapshots.

It does not perform:

- Wi-Fi monitor/promiscuous packet capture.
- Traffic or payload interception.
- Deauthentication, disassociation, or injection.
- Credential collection.
- Automatic association with unknown networks.
- BLE connection, pairing, GATT access, spoofing, or beacon emulation.
- Telemetry, cloud upload, or remote write operations.

Requests to add offensive or unauthorized RF capabilities are outside project
scope.

## Secret Handling

Public builds require no secrets. Known network credentials belong only in
`include/wifi_secrets.h`, which is excluded by `.gitignore`. Templates contain
placeholders only. Credentials must not appear in issues, pull requests,
screenshots, build logs, or firmware binaries distributed publicly.

Because firmware binaries can expose compiled strings, do not publish a binary
built with personal Wi-Fi credentials. Build release artifacts from the public
zero-network configuration.

## Wi-Fi Privacy

Wi-Fi scan metadata includes SSID, RSSI, channel, and security type. SSIDs are
broadcast data but can still identify a home, workplace, or location. Detailed
AP logging is disabled by default. Review screenshots and serial logs before
sharing them.

## BLE Privacy

The application snapshot stores a sanitized display name, RSSI, name state,
and a volatile hash derived from the advertiser address for deduplication. It
does not retain the full address, raw advertisements, service data, or
manufacturer payloads. No historical database exists. Reboot clears the
snapshot.

The hash is not intended as a security identifier and must not be repurposed
for persistent tracking.

## Data Retention

- Radio snapshots exist only in RAM.
- New scans replace prior snapshots.
- Scan metadata is not written to NVS or a filesystem.
- No external transmission exists.
- Reboot clears runtime discovery data.

## Responsible Use

Wireless discovery may be restricted by local law, organizational policy, or
venue rules. Users are responsible for authorization and lawful operation.
Passive behavior does not remove the need for responsible handling of observed
identifiers.

## Vulnerability Reports

Follow the private reporting process in [SECURITY.md](../SECURITY.md). Do not
include real credentials or unnecessary third-party identifiers in a report.
