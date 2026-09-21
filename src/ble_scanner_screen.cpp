#include "ble_scanner_screen.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdio>
#include <cstring>

#include "ble_scanner.h"
#include "rf_scanner_screen.h"

namespace {

constexpr size_t kTopDeviceCount = 8;

constexpr uint32_t kColorBackground = 0x07111F;
constexpr uint32_t kColorYellow = 0xFACC15;
constexpr uint32_t kColorYellowDark = 0xCA8A04;
constexpr uint32_t kColorText = 0xF8FAFC;
constexpr uint32_t kColorMuted = 0x94A3B8;
constexpr uint32_t kColorOnline = 0x4ADE80;
constexpr uint32_t kColorOffline = 0xF59E0B;
constexpr uint32_t kColorError = 0xF87171;

lv_obj_t *overviewScreen = nullptr;
lv_obj_t *topDevicesScreen = nullptr;
lv_obj_t *overviewNetworkStatus = nullptr;
lv_obj_t *topNetworkStatus = nullptr;
lv_obj_t *overviewScanStatus = nullptr;
lv_obj_t *topScanStatus = nullptr;
lv_obj_t *deviceCountLabel = nullptr;
lv_obj_t *strongestLabel = nullptr;
lv_obj_t *nameCountLabel = nullptr;
lv_obj_t *topRows[kTopDeviceCount] = {};
uint32_t renderedGeneration = UINT32_MAX;
BleScanner::State renderedState = BleScanner::State::Idle;
bool renderedDeferred = false;
bool renderInitialized = false;

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
                       lv_align_t align, int16_t x, int16_t y,
                       lv_event_cb_t callback) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, width, 38);
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

void updateStateLabels(BleScanner::State state, bool deferred,
                       bool hasSnapshot) {
  const char *text = "IDLE";
  uint32_t color = kColorMuted;
  if (deferred) {
    text = "WAITING FOR RADIO...";
    color = kColorYellow;
  } else if (state == BleScanner::State::Scanning) {
    text = "SCANNING...";
    color = kColorYellow;
  } else if (state == BleScanner::State::Ready) {
    text = "SCAN READY";
    color = kColorOnline;
  } else if (state == BleScanner::State::Failed) {
    text = hasSnapshot ? "SCAN FAILED - LAST DATA" : "SCAN FAILED";
    color = kColorError;
  }

  lv_label_set_text(overviewScanStatus, text);
  lv_obj_set_style_text_color(overviewScanStatus, lv_color_hex(color),
                              LV_PART_MAIN);
  lv_label_set_text(topScanStatus, text);
  lv_obj_set_style_text_color(topScanStatus, lv_color_hex(color), LV_PART_MAIN);
}

void shortenedName(const BleScanner::Device &device, char *destination,
                   size_t capacity) {
  const char *visible = device.named ? device.name : "Unknown";
  const size_t length = strlen(visible);
  if (length < capacity) {
    snprintf(destination, capacity, "%s", visible);
    return;
  }
  const size_t prefixLength = capacity - 4;
  memcpy(destination, visible, prefixLength);
  memcpy(destination + prefixLength, "...", 4);
}

void renderSnapshot(const BleScanner::Snapshot &snapshot) {
  lv_label_set_text_fmt(deviceCountLabel, "Devices found: %u",
                        snapshot.totalFound);
  lv_label_set_text_fmt(nameCountLabel, "Named devices: %u\nUnknown: %u",
                        snapshot.namedCount, snapshot.unknownCount);

  if (snapshot.storedCount == 0) {
    lv_label_set_text(strongestLabel, "Strongest:\nNone");
  } else {
    const BleScanner::Device &strongest = snapshot.devices[0];
    const char *name = strongest.named ? strongest.name : "Unknown";
    lv_label_set_text_fmt(strongestLabel, "Strongest:\n%.20s\n%d dBm", name,
                          strongest.rssi);
  }

  for (size_t index = 0; index < kTopDeviceCount; ++index) {
    if (index >= snapshot.storedCount) {
      lv_label_set_text(topRows[index], "");
      continue;
    }
    const BleScanner::Device &device = snapshot.devices[index];
    char name[19] = {};
    char row[32] = {};
    shortenedName(device, name, sizeof(name));
    snprintf(row, sizeof(row), "%-18s %4d dBm", name, device.rssi);
    lv_label_set_text(topRows[index], row);
  }
}

void logScreen(const char *name) {
  Serial.printf("F4-B BLE screen=%s free_heap=%u min_free_heap=%u stored=%u\n",
                name, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                BleScanner::snapshot().storedCount);
}

void showTopDevices(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_scr_load(topDevicesScreen);
  logScreen("top-devices");
}

void returnToOverview(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_scr_load(overviewScreen);
  logScreen("overview");
}

void refresh(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  BleScanner::requestScan();
  BleScannerScreen::update();
}

void leaveBle(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  BleScanner::leave();
  RfScannerScreen::showOverview();
}

void buildOverviewScreen() {
  overviewScreen = lv_obj_create(nullptr);
  styleScreen(overviewScreen);

  createLabel(overviewScreen, "BLE ENVIRONMENT", &lv_font_montserrat_16,
              kColorYellow, LV_ALIGN_TOP_LEFT, 8, 10);
  overviewNetworkStatus =
      createLabel(overviewScreen, "OFFLINE", &lv_font_montserrat_14,
                  kColorOffline, LV_ALIGN_TOP_RIGHT, -7, 10);
  overviewScanStatus =
      createLabel(overviewScreen, "IDLE", &lv_font_montserrat_14, kColorMuted,
                  LV_ALIGN_TOP_MID, 0, 40);
  deviceCountLabel =
      createLabel(overviewScreen, "Devices found: 0", &lv_font_montserrat_16,
                  kColorText, LV_ALIGN_TOP_MID, 0, 68);
  strongestLabel =
      createLabel(overviewScreen, "Strongest:\nNone", &lv_font_montserrat_14,
                  kColorText, LV_ALIGN_TOP_MID, 0, 102);
  lv_obj_set_style_text_align(strongestLabel, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  nameCountLabel =
      createLabel(overviewScreen, "Named devices: 0\nUnknown: 0",
                  &lv_font_montserrat_14, kColorMuted, LV_ALIGN_TOP_MID, 0,
                  165);
  lv_obj_set_style_text_align(nameCountLabel, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);

  createButton(overviewScreen, "TOP DEVICES", 144, LV_ALIGN_TOP_MID, 0, 207,
               showTopDevices);
  createButton(overviewScreen, "REFRESH", 106, LV_ALIGN_BOTTOM_LEFT, 8, -8,
               refresh);
  createButton(overviewScreen, "BACK", 106, LV_ALIGN_BOTTOM_RIGHT, -8, -8,
               leaveBle);
}

void buildTopDevicesScreen() {
  topDevicesScreen = lv_obj_create(nullptr);
  styleScreen(topDevicesScreen);

  createLabel(topDevicesScreen, "TOP BLE DEVICES", &lv_font_montserrat_16,
              kColorYellow, LV_ALIGN_TOP_LEFT, 8, 10);
  topNetworkStatus =
      createLabel(topDevicesScreen, "OFFLINE", &lv_font_montserrat_14,
                  kColorOffline, LV_ALIGN_TOP_RIGHT, -7, 10);
  topScanStatus =
      createLabel(topDevicesScreen, "IDLE", &lv_font_montserrat_14,
                  kColorMuted, LV_ALIGN_TOP_MID, 0, 39);

  for (size_t index = 0; index < kTopDeviceCount; ++index) {
    topRows[index] = createLabel(
        topDevicesScreen, "", &lv_font_montserrat_14,
        index % 2 == 0 ? kColorText : kColorMuted, LV_ALIGN_TOP_LEFT, 8,
        static_cast<int16_t>(64 + index * 24));
  }
  createButton(topDevicesScreen, "BACK", 112, LV_ALIGN_BOTTOM_MID, 0, -8,
               returnToOverview);
}

}  // namespace

namespace BleScannerScreen {

void create() {
  buildOverviewScreen();
  buildTopDevicesScreen();
  renderSnapshot(BleScanner::snapshot());
  updateStateLabels(BleScanner::state(), BleScanner::isDeferred(), false);
}

void showOverview() {
  lv_scr_load(overviewScreen);
  BleScanner::requestScan();
  update();
  logScreen("overview");
}

void update() {
  const BleScanner::Snapshot &snapshot = BleScanner::snapshot();
  const BleScanner::State state = BleScanner::state();
  const bool deferred = BleScanner::isDeferred();
  if (renderInitialized && snapshot.generation == renderedGeneration &&
      state == renderedState && deferred == renderedDeferred) {
    return;
  }

  const bool newSnapshot = snapshot.generation != renderedGeneration;
  if (newSnapshot) renderSnapshot(snapshot);
  updateStateLabels(state, deferred, snapshot.totalFound > 0);
  renderedGeneration = snapshot.generation;
  renderedState = state;
  renderedDeferred = deferred;
  renderInitialized = true;
  if (newSnapshot && snapshot.generation > 0) logScreen("updated");
}

void setOnline(bool online) {
  updateNetworkLabel(overviewNetworkStatus, online);
  updateNetworkLabel(topNetworkStatus, online);
}

}  // namespace BleScannerScreen
