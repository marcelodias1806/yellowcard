#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

#include <cstring>

#include "radio_scan_lock.h"

#ifndef YELLOWCARD_WIFI_AP_DEBUG
#define YELLOWCARD_WIFI_AP_DEBUG 0
#endif

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#define YELLOWCARD_HAS_WIFI_SECRETS 1
#else
#define YELLOWCARD_HAS_WIFI_SECRETS 0
namespace YellowCardConfig {
struct WifiNetwork {
  const char *ssid;
  const char *password;
  uint8_t priority;
};
constexpr WifiNetwork kKnownWifiNetworks[1] = {};
constexpr size_t kKnownWifiNetworkCount = 0;
}  // namespace YellowCardConfig
#endif

namespace {

constexpr size_t kMaxKnownNetworks = 4;
constexpr uint32_t kScanTimeoutMs = 15000;
#if YELLOWCARD_WIFI_AP_DEBUG
constexpr uint32_t kScanProgressLogIntervalMs = 1000;
#endif
// Arduino-ESP32 derives its async scan timeout from this value (value * 20).
constexpr uint32_t kScanMaxMsPerChannel = 600;
constexpr bool kPassiveScan = true;
constexpr uint32_t kConnectionTimeoutMs = 12000;
constexpr uint32_t kReconnectIntervalMs = 30000;

static_assert(YellowCardConfig::kKnownWifiNetworkCount <= kMaxKnownNetworks,
              "YellowCard supports at most four known Wi-Fi networks");

enum class State : uint8_t {
  Offline,
  Scanning,
  Connecting,
  Online,
};

struct Selection {
  int8_t configIndex = -1;
  int32_t rssi = -127;
  uint8_t priority = 0;
};

State state = State::Offline;
WifiManager::StatusCallback statusCallback = nullptr;
uint32_t stateStartedAt = 0;
uint32_t nextScanAt = 0;
uint32_t scanAttempts = 0;
bool resumeOnlineAfterScan = false;
WifiManager::ScanStatus currentScanStatus = WifiManager::ScanStatus::Unavailable;
WifiManager::ScanSnapshot latestScan = {};
#if YELLOWCARD_WIFI_AP_DEBUG
uint32_t lastScanProgressLogAt = 0;
#endif

#if YELLOWCARD_WIFI_AP_DEBUG
const char *wifiModeName(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_NULL:
      return "OFF";
    case WIFI_MODE_STA:
      return "STA";
    case WIFI_MODE_AP:
      return "AP";
    case WIFI_MODE_APSTA:
      return "AP+STA";
    default:
      return "UNKNOWN";
  }
}
const char *encryptionName(wifi_auth_mode_t encryption) {
  switch (encryption) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3-PSK";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI-PSK";
    default:
      return "UNKNOWN";
  }
}
#endif

void logHeap() {
  Serial.printf("WiFi heap free=%u min=%u\n", ESP.getFreeHeap(),
                ESP.getMinFreeHeap());
}

void notifyOnline(bool online) {
  if (statusCallback != nullptr) statusCallback(online);
}

void setOffline(uint32_t retryDelayMs) {
  state = State::Offline;
  stateStartedAt = millis();
  nextScanAt = stateStartedAt + retryDelayMs;
  Serial.println("WiFi state=OFFLINE");
  logHeap();
  notifyOnline(false);
}

void finishScan(int16_t scanCount);

WifiManager::Security mapSecurity(wifi_auth_mode_t encryption) {
  switch (encryption) {
    case WIFI_AUTH_OPEN:
      return WifiManager::Security::Open;
    case WIFI_AUTH_WEP:
      return WifiManager::Security::Wep;
    case WIFI_AUTH_WPA_PSK:
      return WifiManager::Security::Wpa;
    case WIFI_AUTH_WPA2_PSK:
      return WifiManager::Security::Wpa2;
    case WIFI_AUTH_WPA_WPA2_PSK:
      return WifiManager::Security::WpaWpa2;
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return WifiManager::Security::Enterprise;
    case WIFI_AUTH_WPA3_PSK:
      return WifiManager::Security::Wpa3;
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return WifiManager::Security::Wpa2Wpa3;
    case WIFI_AUTH_WAPI_PSK:
      return WifiManager::Security::Wapi;
    default:
      return WifiManager::Security::Unknown;
  }
}

void copySafeSsid(char *destination, const String &source) {
  const size_t length =
      source.length() < WifiManager::kMaxSsidLength
          ? source.length()
          : WifiManager::kMaxSsidLength;
  for (size_t index = 0; index < length; ++index) {
    const uint8_t value = static_cast<uint8_t>(source[index]);
    destination[index] = value >= 32 && value <= 126
                             ? static_cast<char>(value)
                             : '?';
  }
  destination[length] = '\0';
}

void insertAccessPoint(const WifiManager::AccessPoint &candidate) {
  size_t insertAt = 0;
  while (insertAt < latestScan.storedCount &&
         latestScan.accessPoints[insertAt].rssi >= candidate.rssi) {
    ++insertAt;
  }
  if (insertAt >= WifiManager::kMaxStoredAccessPoints) return;

  if (latestScan.storedCount < WifiManager::kMaxStoredAccessPoints) {
    ++latestScan.storedCount;
  }
  for (size_t index = latestScan.storedCount - 1;
       index > insertAt; --index) {
    latestScan.accessPoints[index] = latestScan.accessPoints[index - 1];
  }
  latestScan.accessPoints[insertAt] = candidate;
}

void captureScanResults(int16_t scanCount) {
  const uint32_t nextGeneration = latestScan.generation + 1;
  memset(&latestScan, 0, sizeof(latestScan));
  latestScan.generation = nextGeneration;
  latestScan.totalFound = scanCount > 0 ? static_cast<uint16_t>(scanCount) : 0;

  for (int16_t scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
    const wifi_auth_mode_t encryption = WiFi.encryptionType(scanIndex);
    const uint8_t channel = static_cast<uint8_t>(WiFi.channel(scanIndex));
    if (channel >= 1 && channel <= 13) {
      ++latestScan.channelCounts[channel - 1];
    }
    if (encryption == WIFI_AUTH_OPEN) {
      ++latestScan.openCount;
    } else {
      ++latestScan.protectedCount;
    }

    WifiManager::AccessPoint candidate = {};
    copySafeSsid(candidate.ssid, WiFi.SSID(scanIndex));
    candidate.rssi = static_cast<int16_t>(WiFi.RSSI(scanIndex));
    candidate.channel = channel;
    candidate.security = mapSecurity(encryption);
    insertAccessPoint(candidate);
  }

  currentScanStatus = WifiManager::ScanStatus::Ready;
}

void restoreOnlineOrSetOffline() {
  if (resumeOnlineAfterScan) {
    resumeOnlineAfterScan = false;
    state = State::Online;
    return;
  }
  resumeOnlineAfterScan = false;
  setOffline(kReconnectIntervalMs);
}

void startScan() {
  if (state == State::Scanning) {
#if YELLOWCARD_WIFI_AP_DEBUG
    Serial.println("WiFi scan request ignored: scan already in progress");
#endif
    return;
  }

  if (!RadioScanLock::tryAcquire(RadioScanLock::Owner::Wifi)) {
    nextScanAt = millis() + 250;
#if YELLOWCARD_WIFI_AP_DEBUG
    Serial.println("WiFi scan deferred: BLE scan in progress");
#endif
    return;
  }

  if (state == State::Offline) {
    if (scanAttempts > 0) {
      Serial.printf("WiFi reconnection attempt=%u\n", scanAttempts);
    }
    ++scanAttempts;
  }

  const wifi_mode_t modeBefore = WiFi.getMode();
  const bool staModeSet = WiFi.mode(WIFI_STA);
  // WIFI_PS_MIN_MODEM is required by the ESP32 Wi-Fi/BLE coexistence layer.
  const bool sleepEnabled = WiFi.setSleep(WIFI_PS_MIN_MODEM);
  const wifi_mode_t modeAfter = WiFi.getMode();
#if YELLOWCARD_WIFI_AP_DEBUG
  Serial.printf(
      "WiFi mode before=%s(%d) after=%s(%d) sta_set=%s sleep_enabled=%s "
      "status=%d\n",
      wifiModeName(modeBefore), static_cast<int>(modeBefore),
      wifiModeName(modeAfter), static_cast<int>(modeAfter),
      staModeSet ? "yes" : "no", sleepEnabled ? "yes" : "no",
      static_cast<int>(WiFi.status()));
#else
  (void)modeBefore;
  (void)staModeSet;
  (void)sleepEnabled;
  (void)modeAfter;
#endif

  const int16_t pendingResult = WiFi.scanComplete();
#if YELLOWCARD_WIFI_AP_DEBUG
  Serial.printf("WiFi scanComplete raw before start=%d status=%d\n",
                pendingResult, static_cast<int>(WiFi.status()));
#endif
  if (pendingResult == WIFI_SCAN_RUNNING) {
#if YELLOWCARD_WIFI_AP_DEBUG
    Serial.println("WiFi scan diagnostic=RUNNING (existing scan retained)");
#endif
    state = State::Scanning;
    stateStartedAt = millis();
    currentScanStatus = WifiManager::ScanStatus::Scanning;
#if YELLOWCARD_WIFI_AP_DEBUG
    lastScanProgressLogAt = 0;
#endif
    return;
  }
  if (pendingResult >= 0) {
#if YELLOWCARD_WIFI_AP_DEBUG
    Serial.println(
        "WiFi scan diagnostic=COMPLETED (existing results retained)");
#endif
    finishScan(pendingResult);
    return;
  }

  Serial.println("WiFi scan started");
  const int16_t result =
      WiFi.scanNetworks(true, true, kPassiveScan, kScanMaxMsPerChannel);
#if YELLOWCARD_WIFI_AP_DEBUG
  Serial.printf("WiFi scanNetworks raw=%d status=%d\n", result,
                static_cast<int>(WiFi.status()));
#endif
  if (result == WIFI_SCAN_FAILED) {
    Serial.println("WiFi scan failed to start");
    currentScanStatus = WifiManager::ScanStatus::Failed;
    RadioScanLock::release(RadioScanLock::Owner::Wifi);
    restoreOnlineOrSetOffline();
    return;
  }

  if (result >= 0) {
#if YELLOWCARD_WIFI_AP_DEBUG
    Serial.println("WiFi scan diagnostic=COMPLETED_IMMEDIATELY");
#endif
    finishScan(result);
    return;
  }

  state = State::Scanning;
  stateStartedAt = millis();
  currentScanStatus = WifiManager::ScanStatus::Scanning;
#if YELLOWCARD_WIFI_AP_DEBUG
  lastScanProgressLogAt = 0;
  Serial.println("WiFi scan diagnostic=RUNNING");
#endif
}

Selection selectKnownNetwork(int16_t scanCount) {
  Selection selected;

  for (int16_t scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
    const String scannedSsid = WiFi.SSID(scanIndex);
    const int32_t scannedRssi = WiFi.RSSI(scanIndex);

    for (size_t configIndex = 0;
         configIndex < YellowCardConfig::kKnownWifiNetworkCount;
         ++configIndex) {
      const auto &known = YellowCardConfig::kKnownWifiNetworks[configIndex];
      if (known.ssid == nullptr || known.ssid[0] == '\0' ||
          strcmp(scannedSsid.c_str(), known.ssid) != 0) {
        continue;
      }

      const bool higherPriority = known.priority > selected.priority;
      const bool strongerAtSamePriority =
          known.priority == selected.priority && scannedRssi > selected.rssi;
      if (selected.configIndex < 0 || higherPriority ||
          strongerAtSamePriority) {
        selected.configIndex = static_cast<int8_t>(configIndex);
        selected.rssi = scannedRssi;
        selected.priority = known.priority;
      }
    }
  }

  return selected;
}

void beginConnection(const Selection &selected) {
  const auto &network =
      YellowCardConfig::kKnownWifiNetworks[selected.configIndex];

  Serial.printf("WiFi selected ssid=%s priority=%u scan_rssi=%d dBm\n",
                network.ssid, network.priority, selected.rssi);
  WiFi.scanDelete();
  RadioScanLock::release(RadioScanLock::Owner::Wifi);
  resumeOnlineAfterScan = false;
  WiFi.begin(network.ssid, network.password);
  state = State::Connecting;
  stateStartedAt = millis();
  Serial.println("WiFi state=CONNECTING");
  logHeap();
}

void finishScan(int16_t scanCount) {
  Serial.printf("WiFi scan completed APs=%d\n", scanCount);
  captureScanResults(scanCount);
  if (scanCount == 0) {
    WiFi.scanDelete();
    RadioScanLock::release(RadioScanLock::Owner::Wifi);
    restoreOnlineOrSetOffline();
    return;
  }

#if YELLOWCARD_WIFI_AP_DEBUG
  for (int16_t scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
    const wifi_auth_mode_t encryption = WiFi.encryptionType(scanIndex);
    Serial.printf(
        "WiFi AP[%d] ssid=%s rssi=%d dBm channel=%d encryption=%s(%d)\n",
        scanIndex, WiFi.SSID(scanIndex).c_str(), WiFi.RSSI(scanIndex),
        WiFi.channel(scanIndex), encryptionName(encryption),
        static_cast<int>(encryption));
  }
#endif

  if (resumeOnlineAfterScan) {
    WiFi.scanDelete();
    RadioScanLock::release(RadioScanLock::Owner::Wifi);
    resumeOnlineAfterScan = false;
    state = State::Online;
    return;
  }

  const Selection selected = selectKnownNetwork(scanCount);
  if (selected.configIndex < 0) {
    WiFi.scanDelete();
    RadioScanLock::release(RadioScanLock::Owner::Wifi);
    Serial.println("WiFi no known network available");
    restoreOnlineOrSetOffline();
    return;
  }

  beginConnection(selected);
}

void setOnline() {
  state = State::Online;
  const IPAddress ip = WiFi.localIP();
  Serial.println("WiFi state=ONLINE");
  Serial.printf("WiFi IP=%u.%u.%u.%u RSSI=%d dBm\n", ip[0], ip[1], ip[2],
                ip[3], WiFi.RSSI());
  logHeap();
  notifyOnline(true);
}

}  // namespace

namespace WifiManager {

void begin(StatusCallback callback) {
  statusCallback = callback;
  Serial.printf("WiFi configured networks=%u secrets_file=%s\n",
                static_cast<unsigned>(
                    YellowCardConfig::kKnownWifiNetworkCount),
                YELLOWCARD_HAS_WIFI_SECRETS ? "yes" : "no");
  notifyOnline(false);

  if (YellowCardConfig::kKnownWifiNetworkCount == 0) {
    Serial.println("WiFi state=OFFLINE (safe fallback: zero networks)");
    logHeap();
    return;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  startScan();
}

void update() {
  const uint32_t now = millis();
  switch (state) {
    case State::Offline:
      if (YellowCardConfig::kKnownWifiNetworkCount > 0 &&
          static_cast<int32_t>(now - nextScanAt) >= 0) {
        resumeOnlineAfterScan = false;
        startScan();
      }
      break;

    case State::Scanning: {
      const int16_t result = WiFi.scanComplete();
      if (result == WIFI_SCAN_RUNNING) {
#if YELLOWCARD_WIFI_AP_DEBUG
        if (lastScanProgressLogAt == 0 ||
            now - lastScanProgressLogAt >= kScanProgressLogIntervalMs) {
          Serial.printf(
              "WiFi scanComplete raw=%d diagnostic=RUNNING status=%d\n",
              result, static_cast<int>(WiFi.status()));
          lastScanProgressLogAt = now;
        }
#endif
        if (now - stateStartedAt >= kScanTimeoutMs) {
          Serial.println("WiFi scan failed: timeout");
          WiFi.scanDelete();
          RadioScanLock::release(RadioScanLock::Owner::Wifi);
          currentScanStatus = ScanStatus::Failed;
          restoreOnlineOrSetOffline();
        }
        return;
      }
      if (result == WIFI_SCAN_FAILED) {
#if YELLOWCARD_WIFI_AP_DEBUG
        Serial.printf(
            "WiFi scanComplete raw=%d diagnostic=FAILED status=%d mode=%s(%d)\n",
            result, static_cast<int>(WiFi.status()),
            wifiModeName(WiFi.getMode()), static_cast<int>(WiFi.getMode()));
#else
        Serial.println("WiFi scan failed");
#endif
        WiFi.scanDelete();
        RadioScanLock::release(RadioScanLock::Owner::Wifi);
        currentScanStatus = ScanStatus::Failed;
        restoreOnlineOrSetOffline();
        return;
      }
#if YELLOWCARD_WIFI_AP_DEBUG
      Serial.printf("WiFi scanComplete raw=%d diagnostic=COMPLETED status=%d\n",
                    result, static_cast<int>(WiFi.status()));
#endif
      finishScan(result);
      break;
    }

    case State::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        setOnline();
      } else if (now - stateStartedAt >= kConnectionTimeoutMs) {
        Serial.println("WiFi connection timeout");
        WiFi.disconnect(false, false);
        setOffline(kReconnectIntervalMs);
      }
      break;

    case State::Online:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost");
        setOffline(kReconnectIntervalMs);
      }
      break;
  }
}

bool isOnline() {
  return state == State::Online ||
         (state == State::Scanning && resumeOnlineAfterScan);
}

bool requestScan() {
  if (state == State::Scanning || state == State::Connecting) return false;
  if (RadioScanLock::owner() == RadioScanLock::Owner::Ble) return false;

  resumeOnlineAfterScan =
      state == State::Online && WiFi.status() == WL_CONNECTED;
  startScan();
  return state == State::Scanning || currentScanStatus == ScanStatus::Ready;
}

bool canStartBleScan() {
  return state != State::Scanning && state != State::Connecting;
}

ScanStatus scanStatus() { return currentScanStatus; }

const ScanSnapshot &scanSnapshot() { return latestScan; }

const char *securityLabel(Security security) {
  switch (security) {
    case Security::Open:
      return "OPEN";
    case Security::Wep:
      return "WEP";
    case Security::Wpa:
      return "WPA";
    case Security::Wpa2:
      return "WPA2";
    case Security::WpaWpa2:
      return "MIX";
    case Security::Enterprise:
      return "EAP";
    case Security::Wpa3:
      return "WPA3";
    case Security::Wpa2Wpa3:
      return "MIX3";
    case Security::Wapi:
      return "WAPI";
    case Security::Unknown:
    default:
      return "SEC";
  }
}

}  // namespace WifiManager
