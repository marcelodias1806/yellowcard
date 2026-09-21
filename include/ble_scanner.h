#pragma once

#include <stddef.h>
#include <stdint.h>

namespace BleScanner {

constexpr size_t kMaxStoredDevices = 16;
constexpr size_t kMaxDeviceNameLength = 24;
constexpr uint32_t kScanDurationMs = 5000;

enum class State : uint8_t {
  Idle,
  Scanning,
  Ready,
  Failed,
};

struct Device {
  char name[kMaxDeviceNameLength + 1];
  uint32_t identityHash;
  int8_t rssi;
  bool named;
};

struct Snapshot {
  Device devices[kMaxStoredDevices];
  uint16_t totalFound;
  uint16_t namedCount;
  uint16_t unknownCount;
  uint8_t storedCount;
  uint32_t generation;
};

void update();
bool requestScan();
void leave();
State state();
bool isDeferred();
const Snapshot &snapshot();

}  // namespace BleScanner
