#include "app_ui.h"

#include "badge_screen.h"
#include "ble_scanner.h"
#include "ble_scanner_screen.h"
#include "rf_scanner_screen.h"

namespace AppUi {

void create() {
  BadgeScreen::create();
  RfScannerScreen::create();
  BleScannerScreen::create();
}

void update() {
  BleScanner::update();
  RfScannerScreen::update();
  BleScannerScreen::update();
}

void setOnline(bool online) {
  BadgeScreen::setOnline(online);
  RfScannerScreen::setOnline(online);
  BleScannerScreen::setOnline(online);
}

}  // namespace AppUi
