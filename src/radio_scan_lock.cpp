#include "radio_scan_lock.h"

namespace {

RadioScanLock::Owner currentOwner = RadioScanLock::Owner::None;

}  // namespace

namespace RadioScanLock {

bool tryAcquire(Owner requestedOwner) {
  if (currentOwner != Owner::None && currentOwner != requestedOwner) {
    return false;
  }
  currentOwner = requestedOwner;
  return true;
}

void release(Owner releasingOwner) {
  if (currentOwner == releasingOwner) currentOwner = Owner::None;
}

Owner owner() { return currentOwner; }

}  // namespace RadioScanLock
