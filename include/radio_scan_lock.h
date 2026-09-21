#pragma once

#include <stdint.h>

namespace RadioScanLock {

enum class Owner : uint8_t {
  None,
  Wifi,
  Ble,
};

bool tryAcquire(Owner requestedOwner);
void release(Owner releasingOwner);
Owner owner();

}  // namespace RadioScanLock
