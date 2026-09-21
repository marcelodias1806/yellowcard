# Wi-Fi Configuration

YellowCard supports up to four known 2.4 GHz Wi-Fi networks. Credentials are a
local build-time configuration and are never required for an offline build.

## Create the Local Header

Copy the public template:

```sh
cp include/wifi_secrets.example.h include/wifi_secrets.h
```

Edit only `include/wifi_secrets.h`. Its structure must match the interface
consumed by `wifi_manager`:

```cpp
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace YellowCardConfig {

struct WifiNetwork {
  const char *ssid;
  const char *password;
  uint8_t priority;
};

constexpr WifiNetwork kKnownWifiNetworks[] = {
    {"YOUR_WIFI_SSID_1", "YOUR_WIFI_PASSWORD_1", 100},
    {"YOUR_WIFI_SSID_2", "YOUR_WIFI_PASSWORD_2", 75},
};

constexpr size_t kKnownWifiNetworkCount =
    sizeof(kKnownWifiNetworks) / sizeof(kKnownWifiNetworks[0]);

}  // namespace YellowCardConfig
```

Use one to four entries. Do not place real SSIDs or passwords in documentation,
issues, serial logs, screenshots, commits, or pull requests.

## Selection Policy

1. YellowCard performs an asynchronous Wi-Fi scan.
2. Only configured SSIDs are candidates for association.
3. The available network with the highest numeric `priority` wins.
4. If priorities match, the network with stronger RSSI wins.
5. A connection attempt times out after 12 seconds.
6. Failure returns to offline mode; retry scans occur periodically.

The UI and passive scanners remain usable offline. Unknown networks discovered
by the RF screen are displayed but never passed to `WiFi.begin()`.

## Safe Fallback

`wifi_manager.cpp` uses `__has_include("wifi_secrets.h")`. If the local header
does not exist, it compiles an empty configuration with zero known networks.
The firmware reports offline mode and does not need private configuration to
build.

## Repository Safety

The project `.gitignore` excludes:

```text
include/wifi_secrets.h
```

Check before every public commit:

```sh
git check-ignore -v include/wifi_secrets.h
git status --short
git ls-files include/wifi_secrets.h
```

The final command must produce no output. If credentials were ever committed,
removing the current file is not enough: stop publication, rotate the affected
credentials, and review history before proceeding.

## Debug Logging

`YELLOWCARD_WIFI_AP_DEBUG` is `0` by default. Enabling it logs nearby SSIDs and
RF metadata. Sanitize that output before sharing it publicly. Passwords are not
logged by the firmware.
