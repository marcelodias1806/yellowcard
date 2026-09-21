#include "rf_scanner_screen.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdio>
#include <cstring>

#include "badge_screen.h"
#include "ble_scanner.h"
#include "ble_scanner_screen.h"
#include "wifi_manager.h"

namespace {

constexpr size_t kTopNetworkCount = 8;
constexpr size_t kDisplayedChannelCount = 5;

constexpr uint32_t kColorBackground = 0x07111F;
constexpr uint32_t kColorPanel = 0x111C2E;
constexpr uint32_t kColorYellow = 0xFACC15;
constexpr uint32_t kColorYellowDark = 0xCA8A04;
constexpr uint32_t kColorText = 0xF8FAFC;
constexpr uint32_t kColorMuted = 0x94A3B8;
constexpr uint32_t kColorOnline = 0x4ADE80;
constexpr uint32_t kColorOffline = 0xF59E0B;
constexpr uint32_t kColorError = 0xF87171;

lv_obj_t *rfOverviewScreen = nullptr;
lv_obj_t *overviewScreen = nullptr;
lv_obj_t *topNetworksScreen = nullptr;
lv_obj_t *rfNetworkStatus = nullptr;
lv_obj_t *rfWifiCountLabel = nullptr;
lv_obj_t *rfBleCountLabel = nullptr;
lv_obj_t *overviewNetworkStatus = nullptr;
lv_obj_t *topNetworkStatus = nullptr;
lv_obj_t *overviewScanStatus = nullptr;
lv_obj_t *topScanStatus = nullptr;
lv_obj_t *totalLabel = nullptr;
lv_obj_t *strongestLabel = nullptr;
lv_obj_t *securityCountLabel = nullptr;
lv_obj_t *channelsLabel = nullptr;
lv_obj_t *topRows[kTopNetworkCount] = {};
uint32_t renderedGeneration = UINT32_MAX;
WifiManager::ScanStatus renderedStatus = WifiManager::ScanStatus::Unavailable;
bool renderInitialized = false;
uint32_t renderedRfWifiGeneration = UINT32_MAX;
uint32_t renderedRfBleGeneration = UINT32_MAX;

void styleScreen(lv_obj_t *screen) {
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(kColorBackground),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}

lv_obj_t *createLabel(lv_obj_t *parent, const char *text,
                      const lv_font_t *font, uint32_t color,
                      lv_align_t align, int16_t x, int16_t y) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_align(label, align, x, y);
  return label;
}

lv_obj_t *createButton(lv_obj_t *parent, const char *text, lv_coord_t width,
                       int16_t x, lv_event_cb_t callback,
                       lv_align_t align = LV_ALIGN_BOTTOM_MID,
                       int16_t y = -8) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, width, 36);
  lv_obj_align(button, align, x, y);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, lv_color_hex(kColorYellowDark),
                            LV_PART_MAIN);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_center(label);
  return button;
}

void updateNetworkLabel(lv_obj_t *label, bool online) {
  if (label == nullptr) return;
  lv_label_set_text(label, online ? "ONLINE" : "OFFLINE");
  lv_obj_set_style_text_color(
      label, lv_color_hex(online ? kColorOnline : kColorOffline), LV_PART_MAIN);
}

void setScanStatusLabel(lv_obj_t *label, const char *text, uint32_t color) {
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
}

void updateScanStatusLabels(WifiManager::ScanStatus status,
                            bool hasSnapshot) {
  const char *text = "NO SCAN DATA";
  uint32_t color = kColorMuted;
  if (status == WifiManager::ScanStatus::Scanning) {
    text = "SCANNING...";
    color = kColorYellow;
  } else if (status == WifiManager::ScanStatus::Failed) {
    text = hasSnapshot ? "SCAN FAILED - LAST DATA" : "SCAN FAILED";
    color = kColorError;
  } else if (status == WifiManager::ScanStatus::Ready) {
    text = "SCAN READY";
    color = kColorOnline;
  }
  setScanStatusLabel(overviewScanStatus, text, color);
  setScanStatusLabel(topScanStatus, text, color);
}

void appendChannelLine(char *buffer, size_t capacity, size_t &offset,
                       uint8_t channel, uint16_t count,
                       uint16_t maximumCount) {
  const uint8_t barLength = maximumCount == 0
                                ? 0
                                : static_cast<uint8_t>((count * 8U +
                                                        maximumCount - 1U) /
                                                       maximumCount);
  char bar[9] = {};
  for (uint8_t index = 0; index < barLength; ++index) bar[index] = '|';

  const int written = snprintf(buffer + offset, capacity - offset,
                               "CH %2u  %-8s %u\n", channel, bar, count);
  if (written > 0) {
    const size_t added = static_cast<size_t>(written);
    offset += added < capacity - offset ? added : capacity - offset - 1;
  }
}

void renderChannels(const WifiManager::ScanSnapshot &snapshot) {
  struct ChannelRank {
    uint8_t channel;
    uint16_t count;
  } ranks[kDisplayedChannelCount] = {};

  for (uint8_t channel = 1; channel <= 13; ++channel) {
    const uint16_t count = snapshot.channelCounts[channel - 1];
    if (count == 0) continue;
    size_t insertAt = 0;
    while (insertAt < kDisplayedChannelCount &&
           ranks[insertAt].count >= count) {
      ++insertAt;
    }
    if (insertAt == kDisplayedChannelCount) continue;
    for (size_t index = kDisplayedChannelCount - 1; index > insertAt;
         --index) {
      ranks[index] = ranks[index - 1];
    }
    ranks[insertAt] = {channel, count};
  }

  char text[128] = {};
  size_t offset = 0;
  if (ranks[0].count == 0) {
    snprintf(text, sizeof(text), "No occupied channels");
  } else {
    for (const ChannelRank &rank : ranks) {
      if (rank.count == 0) break;
      appendChannelLine(text, sizeof(text), offset, rank.channel, rank.count,
                        ranks[0].count);
    }
  }
  lv_label_set_text(channelsLabel, text);
}

void shortenedSsid(const char *source, char *destination, size_t capacity) {
  const char *visible = source[0] == '\0' ? "<hidden>" : source;
  const size_t length = strlen(visible);
  if (length < capacity) {
    snprintf(destination, capacity, "%s", visible);
    return;
  }
  if (capacity < 5) {
    destination[0] = '\0';
    return;
  }
  const size_t prefixLength = capacity - 4;
  memcpy(destination, visible, prefixLength);
  memcpy(destination + prefixLength, "...", 4);
}

void renderTopNetworks(const WifiManager::ScanSnapshot &snapshot) {
  for (size_t index = 0; index < kTopNetworkCount; ++index) {
    if (index >= snapshot.storedCount) {
      lv_label_set_text(topRows[index], "");
      continue;
    }

    const WifiManager::AccessPoint &network = snapshot.accessPoints[index];
    char ssid[14] = {};
    char row[48] = {};
    shortenedSsid(network.ssid, ssid, sizeof(ssid));
    snprintf(row, sizeof(row), "%-13s %4d C%02u %s", ssid, network.rssi,
             network.channel, WifiManager::securityLabel(network.security));
    lv_label_set_text(topRows[index], row);
  }
}

void renderSnapshot(const WifiManager::ScanSnapshot &snapshot) {
  lv_label_set_text_fmt(totalLabel, "Wi-Fi networks: %u", snapshot.totalFound);
  lv_label_set_text_fmt(securityCountLabel, "OPEN %u   PROTECTED %u",
                        snapshot.openCount, snapshot.protectedCount);

  if (snapshot.storedCount == 0) {
    lv_label_set_text(strongestLabel, "Strongest: none");
  } else {
    const WifiManager::AccessPoint &strongest = snapshot.accessPoints[0];
    const char *ssid = strongest.ssid[0] == '\0' ? "<hidden>" : strongest.ssid;
    lv_label_set_text_fmt(strongestLabel, "Strongest: %.20s\n%d dBm   CH %u",
                          ssid, strongest.rssi, strongest.channel);
  }
  renderChannels(snapshot);
  renderTopNetworks(snapshot);
}

void logRfState(const char *eventName) {
  Serial.printf("F4 RF %s free_heap=%u min_free_heap=%u stored_aps=%u\n",
                eventName, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                WifiManager::scanSnapshot().storedCount);
}

void showTopNetworks(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_scr_load(topNetworksScreen);
  logRfState("screen=top-networks");
}

void showWifiOverview(lv_event_t *event) {
  if (event != nullptr && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_scr_load(overviewScreen);
  WifiManager::requestScan();
  RfScannerScreen::update();
  logRfState("screen=wifi-overview");
}

void showBleOverview(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  BleScannerScreen::showOverview();
}

void showOverviewFromTop(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  showWifiOverview(nullptr);
}

void showRfOverview(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  RfScannerScreen::showOverview();
}

void showBadge(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  BadgeScreen::show();
}

void buildRfOverviewScreen() {
  rfOverviewScreen = lv_obj_create(nullptr);
  styleScreen(rfOverviewScreen);

  createLabel(rfOverviewScreen, "RF ENVIRONMENT", &lv_font_montserrat_20,
              kColorYellow, LV_ALIGN_TOP_MID, 0, 18);
  rfWifiCountLabel =
      createLabel(rfOverviewScreen, "Wi-Fi Networks   0",
                  &lv_font_montserrat_16, kColorText, LV_ALIGN_TOP_LEFT, 28,
                  73);
  rfBleCountLabel =
      createLabel(rfOverviewScreen, "BLE Devices      0",
                  &lv_font_montserrat_16, kColorText, LV_ALIGN_TOP_LEFT, 28,
                  104);

  createButton(rfOverviewScreen, "WI-FI", 184, 0, showWifiOverview,
               LV_ALIGN_TOP_MID, 151);
  createButton(rfOverviewScreen, "BLE", 184, 0, showBleOverview,
               LV_ALIGN_TOP_MID, 199);
  createButton(rfOverviewScreen, "BACK", 112, 0, showBadge);

  rfNetworkStatus =
      createLabel(rfOverviewScreen, "OFFLINE", &lv_font_montserrat_14,
                  kColorOffline, LV_ALIGN_BOTTOM_RIGHT, -8, -53);
}

void buildOverviewScreen() {
  overviewScreen = lv_obj_create(nullptr);
  styleScreen(overviewScreen);

  lv_obj_t *wifiTitle =
      createLabel(overviewScreen, "WI-FI ENVIRONMENT", &lv_font_montserrat_14,
                  kColorYellow, LV_ALIGN_TOP_LEFT, 8, 10);
  lv_obj_set_width(wifiTitle, 148);
  lv_obj_align(wifiTitle, LV_ALIGN_TOP_LEFT, 8, 10);
  overviewNetworkStatus =
      createLabel(overviewScreen, "OFFLINE", &lv_font_montserrat_14,
                  kColorOffline, LV_ALIGN_TOP_RIGHT, -7, 10);
  lv_obj_set_width(overviewNetworkStatus, 64);
  lv_obj_align(overviewNetworkStatus, LV_ALIGN_TOP_RIGHT, -7, 10);
  lv_obj_set_style_text_align(overviewNetworkStatus, LV_TEXT_ALIGN_RIGHT,
                              LV_PART_MAIN);
  overviewScanStatus =
      createLabel(overviewScreen, "NO SCAN DATA", &lv_font_montserrat_14,
                  kColorMuted, LV_ALIGN_TOP_MID, 0, 38);
  totalLabel = createLabel(overviewScreen, "Wi-Fi networks: 0",
                           &lv_font_montserrat_16, kColorText,
                           LV_ALIGN_TOP_MID, 0, 62);
  strongestLabel = createLabel(overviewScreen, "Strongest: none",
                               &lv_font_montserrat_14, kColorText,
                               LV_ALIGN_TOP_MID, 0, 89);
  lv_obj_set_style_text_align(strongestLabel, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  securityCountLabel =
      createLabel(overviewScreen, "OPEN 0   PROTECTED 0",
                  &lv_font_montserrat_14, kColorMuted, LV_ALIGN_TOP_MID, 0,
                  128);
  createLabel(overviewScreen, "BUSIEST CHANNELS", &lv_font_montserrat_14,
              kColorYellow, LV_ALIGN_TOP_LEFT, 14, 153);
  channelsLabel = createLabel(overviewScreen, "No occupied channels",
                              &lv_font_montserrat_14, kColorText,
                              LV_ALIGN_TOP_LEFT, 20, 176);

  createButton(overviewScreen, "TOP NETWORKS", 142, -43, showTopNetworks);
  createButton(overviewScreen, "BACK", 72, 78, showRfOverview);
}

void buildTopNetworksScreen() {
  topNetworksScreen = lv_obj_create(nullptr);
  styleScreen(topNetworksScreen);

  createLabel(topNetworksScreen, "TOP NETWORKS", &lv_font_montserrat_16,
              kColorYellow, LV_ALIGN_TOP_LEFT, 8, 10);
  topNetworkStatus =
      createLabel(topNetworksScreen, "OFFLINE", &lv_font_montserrat_14,
                  kColorOffline, LV_ALIGN_TOP_RIGHT, -7, 10);
  topScanStatus =
      createLabel(topNetworksScreen, "NO SCAN DATA", &lv_font_montserrat_14,
                  kColorMuted, LV_ALIGN_TOP_MID, 0, 37);

  for (size_t index = 0; index < kTopNetworkCount; ++index) {
    topRows[index] = createLabel(
        topNetworksScreen, "", &lv_font_montserrat_14,
        index % 2 == 0 ? kColorText : kColorMuted, LV_ALIGN_TOP_LEFT, 8,
        static_cast<int16_t>(62 + index * 24));
  }
  createButton(topNetworksScreen, "BACK", 112, 0, showOverviewFromTop);
}

}  // namespace

namespace RfScannerScreen {

void create() {
  buildRfOverviewScreen();
  buildOverviewScreen();
  buildTopNetworksScreen();
  renderSnapshot(WifiManager::scanSnapshot());
  updateScanStatusLabels(WifiManager::scanStatus(), false);
}

void showOverview() {
  lv_scr_load(rfOverviewScreen);
  update();
  logRfState("screen=overview");
}

void update() {
  const WifiManager::ScanSnapshot &snapshot = WifiManager::scanSnapshot();
  const WifiManager::ScanStatus status = WifiManager::scanStatus();
  const BleScanner::Snapshot &bleSnapshot = BleScanner::snapshot();
  if (snapshot.generation != renderedRfWifiGeneration ||
      bleSnapshot.generation != renderedRfBleGeneration) {
    lv_label_set_text_fmt(rfWifiCountLabel, "Wi-Fi Networks   %u",
                          snapshot.totalFound);
    lv_label_set_text_fmt(rfBleCountLabel, "BLE Devices      %u",
                          bleSnapshot.totalFound);
    renderedRfWifiGeneration = snapshot.generation;
    renderedRfBleGeneration = bleSnapshot.generation;
  }
  if (renderInitialized && snapshot.generation == renderedGeneration &&
      status == renderedStatus) {
    return;
  }

  const bool newSnapshot = snapshot.generation != renderedGeneration;
  if (newSnapshot) renderSnapshot(snapshot);
  updateScanStatusLabels(status, snapshot.generation > 0);
  renderedGeneration = snapshot.generation;
  renderedStatus = status;
  renderInitialized = true;
  if (newSnapshot && snapshot.generation > 0) logRfState("updated");
}

void setOnline(bool online) {
  updateNetworkLabel(rfNetworkStatus, online);
  updateNetworkLabel(overviewNetworkStatus, online);
  updateNetworkLabel(topNetworkStatus, online);
}

}  // namespace RfScannerScreen
