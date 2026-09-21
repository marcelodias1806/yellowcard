#pragma once

#include <stddef.h>
#include <stdint.h>

namespace YellowCardConfig {

struct WifiNetwork {
  const char *ssid;
  const char *password;
  uint8_t priority;
};

// Copy this file to wifi_secrets.h and replace placeholders locally.
// Never commit wifi_secrets.h or real network credentials.
constexpr WifiNetwork kKnownWifiNetworks[] = {
    {"YOUR_WIFI_SSID_1", "YOUR_WIFI_PASSWORD_1", 100},
    {"YOUR_WIFI_SSID_2", "YOUR_WIFI_PASSWORD_2", 75},
    {"YOUR_WIFI_SSID_3", "YOUR_WIFI_PASSWORD_3", 50},
    {"YOUR_WIFI_SSID_4", "YOUR_WIFI_PASSWORD_4", 25},
};

constexpr size_t kKnownWifiNetworkCount =
    sizeof(kKnownWifiNetworks) / sizeof(kKnownWifiNetworks[0]);

}  // namespace YellowCardConfig
