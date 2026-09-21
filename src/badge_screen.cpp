#include "badge_screen.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstring>

#include "rf_scanner_screen.h"

namespace {

// Change this single constant to point the badge QR Code to another contact URL.
constexpr char kQrContactUrl[] = "https://tecnocorp.com.br/marcelo";
constexpr char kLinkedInShort[] = "linkedin.com/in/mdiasx";
constexpr char kInstagramShort[] = "instagram.com/binbash.sh";

constexpr lv_coord_t kQrSize = 116;
constexpr lv_coord_t kQrPanelSize = 148;

constexpr uint32_t kColorBackground = 0x07111F;
constexpr uint32_t kColorPanel = 0x111C2E;
constexpr uint32_t kColorYellow = 0xFACC15;
constexpr uint32_t kColorYellowDark = 0xCA8A04;
constexpr uint32_t kColorText = 0xF8FAFC;
constexpr uint32_t kColorMuted = 0x94A3B8;

lv_obj_t *mainScreen = nullptr;
lv_obj_t *contactsScreen = nullptr;
lv_obj_t *mainNetworkStatus = nullptr;
lv_obj_t *contactsNetworkStatus = nullptr;

void styleScreen(lv_obj_t *screen) {
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(kColorBackground),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}

lv_obj_t *createLabel(lv_obj_t *parent, const char *text,
                      const lv_font_t *font, uint32_t color, int16_t y) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

lv_obj_t *createButton(lv_obj_t *parent, const char *text, int16_t y,
                       lv_event_cb_t callback, lv_coord_t width = 154,
                       int16_t x = 0,
                       const lv_font_t *font = &lv_font_montserrat_16) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, width, 40);
  lv_obj_align(button, LV_ALIGN_TOP_MID, x, y);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, lv_color_hex(kColorYellowDark),
                            LV_PART_MAIN);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_center(label);
  return button;
}

void logScreenChange(const char *screenName) {
  Serial.printf("F4 screen=%s free_heap=%u min_free_heap=%u\n", screenName,
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void updateNetworkLabel(lv_obj_t *label, bool online) {
  if (label == nullptr) return;
  lv_label_set_text(label, online ? "ONLINE" : "OFFLINE");
  lv_obj_set_style_text_color(
      label, lv_color_hex(online ? 0x4ADE80 : 0xF59E0B), LV_PART_MAIN);
}

void showContacts(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_scr_load(contactsScreen);
  logScreenChange("contacts");
}

void showBadge(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_scr_load(mainScreen);
  logScreenChange("badge");
}

void showRfScanner(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  RfScannerScreen::showOverview();
}

void createQrCode(lv_obj_t *parent) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_size(panel, kQrPanelSize, kQrPanelSize);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 86);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(panel, 16, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 8, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(panel, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_add_event_cb(panel, showContacts, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *qrcode = lv_qrcode_create(panel, kQrSize, lv_color_black(),
                                      lv_color_white());
  lv_obj_center(qrcode);
  lv_obj_add_flag(qrcode, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(qrcode, showContacts, LV_EVENT_CLICKED, nullptr);

  const lv_res_t result =
      lv_qrcode_update(qrcode, kQrContactUrl, strlen(kQrContactUrl));
  if (result != LV_RES_OK) {
    Serial.println("F4 QR generation failed");
  }
}

void createContactCard(lv_obj_t *parent, const char *network,
                       const char *address, int16_t y) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, 216, 66);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(0x26354D), LV_PART_MAIN);
  lv_obj_set_style_bg_color(card, lv_color_hex(kColorPanel), LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(card);
  lv_label_set_text(title, network);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(title, lv_color_hex(kColorYellow), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, -2);

  lv_obj_t *value = lv_label_create(card);
  lv_label_set_text(value, address);
  lv_label_set_long_mode(value, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(value, 194);
  lv_obj_set_style_text_font(value, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(value, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(value, LV_ALIGN_BOTTOM_MID, 0, 1);
}

void buildMainScreen() {
  mainScreen = lv_scr_act();
  styleScreen(mainScreen);

  createLabel(mainScreen, "Marcelo Dias", &lv_font_montserrat_24, kColorText,
              25);
  createLabel(mainScreen, "Cloud & Cybersecurity", &lv_font_montserrat_14,
              kColorMuted, 59);

  mainNetworkStatus = createLabel(mainScreen, "OFFLINE",
                                  &lv_font_montserrat_14, 0xF59E0B, 0);
  lv_obj_align(mainNetworkStatus, LV_ALIGN_TOP_RIGHT, -8, 5);

  createQrCode(mainScreen);
  createLabel(mainScreen, "in/mdiasx  |  @binbash.sh", &lv_font_montserrat_14,
              kColorMuted, 237);
  createButton(mainScreen, "CONTACTS", 259, showContacts, 112, -60,
               &lv_font_montserrat_14);
  createButton(mainScreen, "RF", 259, showRfScanner, 112, 60,
               &lv_font_montserrat_14);
}

void buildContactsScreen() {
  contactsScreen = lv_obj_create(nullptr);
  styleScreen(contactsScreen);

  createLabel(contactsScreen, "CONTACTS", &lv_font_montserrat_20,
              kColorYellow, 14);
  createLabel(contactsScreen, "Marcelo Dias", &lv_font_montserrat_16,
              kColorText, 47);
  createContactCard(contactsScreen, "LINKEDIN", kLinkedInShort, 78);
  createContactCard(contactsScreen, "INSTAGRAM", kInstagramShort, 153);
  createButton(contactsScreen, "BACK", 242, showBadge);

  contactsNetworkStatus = createLabel(contactsScreen, "OFFLINE",
                                      &lv_font_montserrat_14, 0xF59E0B, 0);
  lv_obj_align(contactsNetworkStatus, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
}

}  // namespace

namespace BadgeScreen {

void create() {
  buildMainScreen();
  buildContactsScreen();
  lv_scr_load(mainScreen);

  Serial.printf("F4-B badge ready. QR=%s free_heap=%u min_free_heap=%u\n",
                kQrContactUrl, ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void show() {
  lv_scr_load(mainScreen);
  logScreenChange("badge");
}

void setOnline(bool online) {
  updateNetworkLabel(mainNetworkStatus, online);
  updateNetworkLabel(contactsNetworkStatus, online);
}

}  // namespace BadgeScreen
