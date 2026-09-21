#include "ble_scanner.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_err.h>
#include <esp_wifi.h>

#include <cstring>
#include <string>

#include "radio_scan_lock.h"
#include "wifi_manager.h"

#ifndef YELLOWCARD_BLE_DEVICE_DEBUG
#define YELLOWCARD_BLE_DEVICE_DEBUG 0
#endif

namespace {

constexpr uint16_t kScanIntervalMs = 100;
constexpr uint16_t kScanWindowMs = 50;
constexpr uint32_t kWifiPowerSaveRetryMs = 250;
constexpr uint32_t kWifiPowerSaveTimeoutMs = 5000;

enum class InitResult : uint8_t {
  Ready,
  Deferred,
  WifiPowerSaveFailed,
  NimbleFailed,
};

BleScanner::State currentState = BleScanner::State::Idle;
BleScanner::Snapshot latestSnapshot = {};
NimBLEScan *scanner = nullptr;
bool bleInitialized = false;
bool scanRequested = false;
bool stopRequested = false;
bool leaveRequested = false;
volatile bool completionPending = false;
volatile int completionReason = 0;
volatile uint32_t completionSignaledAt = 0;
uint32_t scanStartedAt = 0;
uint32_t stopRequestedAt = 0;
uint32_t wifiPowerSaveStartedAt = 0;
uint32_t nextWifiPowerSaveAttemptAt = 0;
uint32_t snapshotGeneration = 0;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;

void copySafeName(char *destination, const std::string &source) {
  const size_t length =
      source.length() < BleScanner::kMaxDeviceNameLength
          ? source.length()
          : BleScanner::kMaxDeviceNameLength;
  for (size_t index = 0; index < length; ++index) {
    const uint8_t value = static_cast<uint8_t>(source[index]);
    destination[index] = value >= 32 && value <= 126
                             ? static_cast<char>(value)
                             : '?';
  }
  destination[length] = '\0';
}

uint32_t deviceIdentityHash(const NimBLEAddress &address) {
  constexpr uint32_t kFnvOffset = 2166136261UL;
  constexpr uint32_t kFnvPrime = 16777619UL;
  uint32_t hash = kFnvOffset;
  const uint8_t *value = address.getVal();
  for (size_t index = 0; index < BLE_DEV_ADDR_LEN; ++index) {
    hash = (hash ^ value[index]) * kFnvPrime;
  }
  return (hash ^ address.getType()) * kFnvPrime;
}

void sortStoredDevices() {
  for (size_t index = 1; index < latestSnapshot.storedCount; ++index) {
    const BleScanner::Device device = latestSnapshot.devices[index];
    size_t destination = index;
    while (destination > 0 &&
           latestSnapshot.devices[destination - 1].rssi < device.rssi) {
      latestSnapshot.devices[destination] =
          latestSnapshot.devices[destination - 1];
      --destination;
    }
    latestSnapshot.devices[destination] = device;
  }
}

void storeDevice(const BleScanner::Device &candidate) {
  for (size_t index = 0; index < latestSnapshot.storedCount; ++index) {
    BleScanner::Device &existing = latestSnapshot.devices[index];
    if (existing.identityHash != candidate.identityHash) continue;

    if (!existing.named && candidate.named) {
      memcpy(existing.name, candidate.name, sizeof(existing.name));
      existing.named = true;
      ++latestSnapshot.namedCount;
      if (latestSnapshot.unknownCount > 0) --latestSnapshot.unknownCount;
    }
    if (candidate.rssi > existing.rssi) existing.rssi = candidate.rssi;
    sortStoredDevices();
    return;
  }

  ++latestSnapshot.totalFound;
  if (candidate.named) {
    ++latestSnapshot.namedCount;
  } else {
    ++latestSnapshot.unknownCount;
  }

  size_t insertAt = 0;
  while (insertAt < latestSnapshot.storedCount &&
         latestSnapshot.devices[insertAt].rssi >= candidate.rssi) {
    ++insertAt;
  }
  if (insertAt >= BleScanner::kMaxStoredDevices) return;

  if (latestSnapshot.storedCount < BleScanner::kMaxStoredDevices) {
    ++latestSnapshot.storedCount;
  }
  for (size_t index = latestSnapshot.storedCount - 1; index > insertAt;
       --index) {
    latestSnapshot.devices[index] = latestSnapshot.devices[index - 1];
  }
  latestSnapshot.devices[insertAt] = candidate;
}

class PassiveScanCallbacks final : public NimBLEScanCallbacks {
 public:
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
    BleScanner::Device candidate = {};
    candidate.identityHash =
        deviceIdentityHash(advertisedDevice->getAddress());
    candidate.rssi = advertisedDevice->getRSSI();
    candidate.named = advertisedDevice->haveName();
    if (candidate.named) {
      copySafeName(candidate.name, advertisedDevice->getName());
      candidate.named = candidate.name[0] != '\0';
    }

    portENTER_CRITICAL(&snapshotMux);
    storeDevice(candidate);
    portEXIT_CRITICAL(&snapshotMux);

#if YELLOWCARD_BLE_DEVICE_DEBUG
    Serial.printf("BLE discovered name=%s rssi=%d\n",
                  candidate.named ? candidate.name : "unknown",
                  candidate.rssi);
#endif
  }

  void onScanEnd(const NimBLEScanResults &, int reason) override {
    portENTER_CRITICAL(&snapshotMux);
    completionReason = reason;
    completionSignaledAt = millis();
    completionPending = true;
    portEXIT_CRITICAL(&snapshotMux);
  }
};

PassiveScanCallbacks scanCallbacks;

void cleanupCompletedScan() {
  Serial.printf("BLE cleanup begin free_heap=%u min_free_heap=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
  if (scanner != nullptr) scanner->clearResults();
  RadioScanLock::release(RadioScanLock::Owner::Ble);
  Serial.printf("BLE cleanup ok free_heap=%u min_free_heap=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void failScan(const char *message) {
  Serial.printf("BLE scan failed: %s free_heap=%u min_free_heap=%u\n", message,
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
  cleanupCompletedScan();
  scanRequested = false;
  stopRequested = false;
  leaveRequested = false;
  portENTER_CRITICAL(&snapshotMux);
  memset(&latestSnapshot, 0, sizeof(latestSnapshot));
  latestSnapshot.generation = ++snapshotGeneration;
  portEXIT_CRITICAL(&snapshotMux);
  currentState = BleScanner::State::Failed;
  Serial.println("BLE state=FAILED");
}

bool isWifiDriverPending(esp_err_t result) {
  return result == ESP_ERR_WIFI_NOT_INIT ||
         result == ESP_ERR_WIFI_NOT_STARTED;
}

InitResult prepareWifiPowerSave() {
  wifi_mode_t mode = WiFi.getMode();
  wl_status_t status = WiFi.status();
  bool staActive = (mode & WIFI_MODE_STA) != 0;
  Serial.printf(
      "BLE WiFi PS check mode=%d status=%d connected=%s sta_active=%s\n",
      static_cast<int>(mode), static_cast<int>(status),
      status == WL_CONNECTED ? "yes" : "no", staActive ? "yes" : "no");

  if (!staActive) {
    const bool staRequested = WiFi.mode(WIFI_STA);
    mode = WiFi.getMode();
    status = WiFi.status();
    staActive = (mode & WIFI_MODE_STA) != 0;
    Serial.printf(
        "BLE WiFi STA request result=%s mode=%d status=%d sta_active=%s\n",
        staRequested ? "ok" : "failed", static_cast<int>(mode),
        static_cast<int>(status), staActive ? "yes" : "no");
    if (!staActive) return InitResult::Deferred;
  }

  const esp_err_t setResult = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  wifi_ps_type_t actualPowerSave = WIFI_PS_NONE;
  const esp_err_t getResult = esp_wifi_get_ps(&actualPowerSave);
  Serial.printf(
      "BLE WiFi PS set_rc=%d(%s) get_rc=%d(%s) value=%d expected=%d "
      "mode=%d status=%d connected=%s\n",
      static_cast<int>(setResult), esp_err_to_name(setResult),
      static_cast<int>(getResult), esp_err_to_name(getResult),
      static_cast<int>(actualPowerSave), static_cast<int>(WIFI_PS_MIN_MODEM),
      static_cast<int>(WiFi.getMode()), static_cast<int>(WiFi.status()),
      WiFi.status() == WL_CONNECTED ? "yes" : "no");

  if (setResult == ESP_OK && getResult == ESP_OK &&
      actualPowerSave == WIFI_PS_MIN_MODEM) {
    return InitResult::Ready;
  }
  if (isWifiDriverPending(setResult) || isWifiDriverPending(getResult)) {
    return InitResult::Deferred;
  }
  return InitResult::WifiPowerSaveFailed;
}

InitResult ensureBleInitialized() {
  if (bleInitialized) return InitResult::Ready;

  Serial.printf("BLE state=INITIALIZING free_heap=%u min_free_heap=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
  Serial.println("BLE init begin");

  const InitResult powerSaveResult = prepareWifiPowerSave();
  if (powerSaveResult != InitResult::Ready) return powerSaveResult;

  if (!NimBLEDevice::init("")) {
    Serial.println("BLE init failed: NimBLEDevice::init");
    return InitResult::NimbleFailed;
  }

  bleInitialized = true;
  Serial.printf("BLE init ok free_heap=%u min_free_heap=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
  return InitResult::Ready;
}

void startRequestedScan() {
  if (!WifiManager::canStartBleScan()) return;
  if (static_cast<int32_t>(millis() - nextWifiPowerSaveAttemptAt) < 0) return;
  if (!RadioScanLock::tryAcquire(RadioScanLock::Owner::Ble)) return;

  const InitResult initResult = ensureBleInitialized();
  if (initResult == InitResult::Deferred) {
    RadioScanLock::release(RadioScanLock::Owner::Ble);
    if (wifiPowerSaveStartedAt == 0) wifiPowerSaveStartedAt = millis();
    if (millis() - wifiPowerSaveStartedAt >= kWifiPowerSaveTimeoutMs) {
      failScan("WiFi modem sleep timeout");
      return;
    }
    nextWifiPowerSaveAttemptAt = millis() + kWifiPowerSaveRetryMs;
    Serial.println("BLE state=WAITING_WIFI_POWER_SAVE");
    return;
  }
  if (initResult == InitResult::WifiPowerSaveFailed) {
    failScan("WiFi modem sleep configuration");
    return;
  }
  if (initResult == InitResult::NimbleFailed) {
    failScan("NimBLE init");
    return;
  }
  wifiPowerSaveStartedAt = 0;
  nextWifiPowerSaveAttemptAt = 0;

  portENTER_CRITICAL(&snapshotMux);
  memset(&latestSnapshot, 0, sizeof(latestSnapshot));
  latestSnapshot.generation = snapshotGeneration;
  portEXIT_CRITICAL(&snapshotMux);
  completionPending = false;
  completionReason = 0;
  completionSignaledAt = 0;
  stopRequested = false;
  leaveRequested = false;

  if (scanner == nullptr) scanner = NimBLEDevice::getScan();
  if (scanner == nullptr) {
    failScan("scanner unavailable");
    return;
  }
  Serial.println("BLE scan object ready");

  scanner->setScanCallbacks(&scanCallbacks, false);
  scanner->setActiveScan(false);
  scanner->setInterval(kScanIntervalMs);
  scanner->setWindow(kScanWindowMs);
  scanner->setMaxResults(0);
  scanner->clearResults();

  Serial.printf("BLE scan start duration_ms=%u free_heap=%u min_free_heap=%u\n",
                BleScanner::kScanDurationMs, ESP.getFreeHeap(),
                ESP.getMinFreeHeap());
  if (!scanner->start(BleScanner::kScanDurationMs, false, false)) {
    failScan("start rejected");
    return;
  }

  scanRequested = false;
  scanStartedAt = millis();
  currentState = BleScanner::State::Scanning;
  Serial.println("BLE state=SCANNING");
}

void finishScan() {
  int reason = 0;
  portENTER_CRITICAL(&snapshotMux);
  reason = completionReason;
  completionPending = false;
  portEXIT_CRITICAL(&snapshotMux);

  const uint32_t elapsed = millis() - scanStartedAt;
  const bool stopped = stopRequested;
  const bool successful = reason == 0;
  uint16_t discoveredTotal = 0;
  uint8_t storedCount = 0;
  portENTER_CRITICAL(&snapshotMux);
  discoveredTotal = latestSnapshot.totalFound;
  storedCount = latestSnapshot.storedCount;
  portEXIT_CRITICAL(&snapshotMux);
  Serial.printf(
      "BLE scan end reason=%d discovered_total=%u stored=%u free_heap=%u "
      "min_free_heap=%u\n",
      reason, discoveredTotal, storedCount, ESP.getFreeHeap(),
      ESP.getMinFreeHeap());
  Serial.println("BLE scan stop");
  cleanupCompletedScan();
  stopRequested = false;

  if (stopped) {
    portENTER_CRITICAL(&snapshotMux);
    memset(&latestSnapshot, 0, sizeof(latestSnapshot));
    latestSnapshot.generation = ++snapshotGeneration;
    portEXIT_CRITICAL(&snapshotMux);
    currentState = BleScanner::State::Idle;
    Serial.println("BLE state=IDLE");
    Serial.printf("BLE scan stopped time_ms=%u free_heap=%u min_free_heap=%u\n",
                  elapsed, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    return;
  }

  if (!successful) {
    portENTER_CRITICAL(&snapshotMux);
    latestSnapshot.generation = ++snapshotGeneration;
    portEXIT_CRITICAL(&snapshotMux);
    currentState = BleScanner::State::Failed;
    Serial.println("BLE state=FAILED");
    Serial.printf(
        "BLE scan failed reason=%d discovered_total=%u stored=%u time_ms=%u "
        "free_heap=%u min_free_heap=%u\n",
        reason, discoveredTotal, storedCount, elapsed, ESP.getFreeHeap(),
        ESP.getMinFreeHeap());
    return;
  }

  portENTER_CRITICAL(&snapshotMux);
  latestSnapshot.generation = ++snapshotGeneration;
  portEXIT_CRITICAL(&snapshotMux);
  currentState = BleScanner::State::Ready;
  Serial.println("BLE state=READY");
  Serial.printf(
      "BLE scan complete discovered=%u stored=%u time_ms=%u free_heap=%u "
      "min_free_heap=%u\n",
      latestSnapshot.totalFound, latestSnapshot.storedCount, elapsed,
      ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

}  // namespace

namespace BleScanner {

void update() {
  if (completionPending && millis() - completionSignaledAt >= 20) {
    finishScan();
    return;
  }

  if (stopRequested && scanner != nullptr && !scanner->isScanning() &&
      millis() - stopRequestedAt >= 50) {
    cleanupCompletedScan();
    stopRequested = false;
    portENTER_CRITICAL(&snapshotMux);
    memset(&latestSnapshot, 0, sizeof(latestSnapshot));
    latestSnapshot.generation = ++snapshotGeneration;
    portEXIT_CRITICAL(&snapshotMux);
    currentState = State::Idle;
    Serial.println("BLE state=IDLE");
    return;
  }

  if (leaveRequested) {
    leaveRequested = false;
    scanRequested = false;
    if (currentState == State::Scanning && scanner != nullptr) {
      stopRequested = true;
      stopRequestedAt = millis();
      Serial.println("BLE scan stop requested");
      if (!scanner->stop()) failScan("stop rejected");
      return;
    }
    currentState = State::Idle;
    Serial.println("BLE state=IDLE");
    return;
  }

  if (scanRequested && currentState != State::Scanning) startRequestedScan();
}

bool requestScan() {
  if (currentState == State::Scanning || scanRequested) return false;
  leaveRequested = false;
  wifiPowerSaveStartedAt = 0;
  nextWifiPowerSaveAttemptAt = 0;
  scanRequested = true;
  currentState = State::Idle;
  Serial.println("BLE state=IDLE scan=requested");
  return true;
}

void leave() {
  scanRequested = false;
  leaveRequested = true;
}

State state() { return currentState; }

bool isDeferred() { return scanRequested; }

const Snapshot &snapshot() { return latestSnapshot; }

}  // namespace BleScanner
