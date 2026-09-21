#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WifiManager {

constexpr size_t kMaxStoredAccessPoints = 16;
constexpr size_t kMaxSsidLength = 32;

enum class ScanStatus : uint8_t {
  Unavailable,
  Scanning,
  Ready,
  Failed,
};

enum class Security : uint8_t {
  Open,
  Wep,
  Wpa,
  Wpa2,
  WpaWpa2,
  Enterprise,
  Wpa3,
  Wpa2Wpa3,
  Wapi,
  Unknown,
};

struct AccessPoint {
  char ssid[kMaxSsidLength + 1];
  int16_t rssi;
  uint8_t channel;
  Security security;
};

struct ScanSnapshot {
  AccessPoint accessPoints[kMaxStoredAccessPoints];
  uint16_t channelCounts[13];
  uint16_t totalFound;
  uint16_t openCount;
  uint16_t protectedCount;
  uint8_t storedCount;
  uint32_t generation;
};

using StatusCallback = void (*)(bool online);

void begin(StatusCallback callback);
void update();
bool isOnline();
bool requestScan();
bool canStartBleScan();
ScanStatus scanStatus();
const ScanSnapshot &scanSnapshot();
const char *securityLabel(Security security);

}  // namespace WifiManager
