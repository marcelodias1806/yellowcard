#include <Arduino.h>
#include <esp_system.h>

#include "app_ui.h"
#include "lvgl_port.h"
#include "wifi_manager.h"

namespace {

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external-pin";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "other-watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    case ESP_RST_UNKNOWN:
    default: return "unknown";
  }
}

void printHardwareProbe() {
  const esp_reset_reason_t resetReason = esp_reset_reason();

  Serial.println();
  Serial.println("=== ESP32 hardware probe ===");
  Serial.printf("Chip: %s\n", ESP.getChipModel());
  Serial.printf("Revision: %u\n", ESP.getChipRevision());
  Serial.printf("CPU: %u MHz, %u core(s)\n", ESP.getCpuFreqMHz(),
                ESP.getChipCores());
  Serial.printf("Flash: %u bytes (%u MHz)\n", ESP.getFlashChipSize(),
                ESP.getFlashChipSpeed() / 1000000U);
  Serial.printf("Heap: %u bytes total, %u bytes free\n", ESP.getHeapSize(),
                ESP.getFreeHeap());
  Serial.printf("PSRAM: %u bytes total, %u bytes free\n", ESP.getPsramSize(),
                ESP.getFreePsram());
  Serial.printf("Reset reason: %s (%d)\n", resetReasonName(resetReason),
                static_cast<int>(resetReason));
  Serial.println("============================");
}

void onWifiStatusChanged(bool online) { AppUi::setOnline(online); }

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Firmware: YellowCard F4-B");
  printHardwareProbe();
  LvglPort::begin();
  AppUi::create();
  WifiManager::begin(onWifiStatusChanged);
}

void loop() {
  WifiManager::update();
  AppUi::update();
  LvglPort::runOnce();
  delay(5);
}
