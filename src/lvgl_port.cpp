#include "lvgl_port.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

namespace {

constexpr uint8_t kTouchSclk = 25;
constexpr uint8_t kTouchCs = 33;
constexpr uint8_t kTouchMosi = 32;
constexpr uint8_t kTouchMiso = 39;
constexpr uint8_t kTouchIrq = 36;

// F0-C calibration. These remain centralized for later physical refinement.
constexpr int16_t kTouchRawXMin = 300;
constexpr int16_t kTouchRawXMax = 3900;
constexpr int16_t kTouchRawYMin = 200;
constexpr int16_t kTouchRawYMax = 3700;
constexpr bool kTouchSwapXy = false;
constexpr bool kTouchInvertX = false;
constexpr bool kTouchInvertY = false;

TFT_eSPI display;
SPIClass touchSpi(HSPI);
XPT2046_Touchscreen touch(kTouchCs, kTouchIrq);

// 240 x 20 x RGB565 = 9,600 bytes (6.25% of a full 153,600-byte buffer).
static lv_color_t drawBufferPixels[LvglPort::kScreenWidth *
                                   LvglPort::kDrawBufferLines];
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t displayDriver;
static lv_indev_drv_t inputDriver;

int32_t normalizeRaw(int32_t value, int32_t minimum, int32_t maximum) {
  value = constrain(value, minimum, maximum);
  return (value - minimum) * 10000L / (maximum - minimum);
}

void mapTouch(int16_t rawX, int16_t rawY, lv_point_t &point) {
  int32_t normalizedX = normalizeRaw(rawX, kTouchRawXMin, kTouchRawXMax);
  int32_t normalizedY = normalizeRaw(rawY, kTouchRawYMin, kTouchRawYMax);

  if (kTouchSwapXy) {
    const int32_t temporary = normalizedX;
    normalizedX = normalizedY;
    normalizedY = temporary;
  }
  if (kTouchInvertX) normalizedX = 10000L - normalizedX;
  if (kTouchInvertY) normalizedY = 10000L - normalizedY;

  point.x = normalizedX * (LvglPort::kScreenWidth - 1L) / 10000L;
  point.y = normalizedY * (LvglPort::kScreenHeight - 1L) / 10000L;
}

void flushDisplay(lv_disp_drv_t *driver, const lv_area_t *area,
                  lv_color_t *colors) {
  const uint32_t width = area->x2 - area->x1 + 1;
  const uint32_t height = area->y2 - area->y1 + 1;

  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, width, height);
  display.pushColors(reinterpret_cast<uint16_t *>(&colors->full),
                     width * height, true);
  display.endWrite();
  lv_disp_flush_ready(driver);
}

void readTouch(lv_indev_drv_t *, lv_indev_data_t *data) {
  if (!touch.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  const TS_Point raw = touch.getPoint();
  mapTouch(raw.x, raw.y, data->point);
  data->state = LV_INDEV_STATE_PRESSED;
}

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  display.init();
  display.setRotation(0);
  display.fillScreen(TFT_BLACK);

  Serial.printf("Display: ST7789 %dx%d, rotation=0\n", display.width(),
                display.height());
}

void initTouch() {
  touchSpi.begin(kTouchSclk, kTouchMiso, kTouchMosi, kTouchCs);
  touch.begin(touchSpi);
  touch.setRotation(0);

  Serial.printf("Touch: XPT2046 CLK=%u MOSI=%u MISO=%u CS=%u IRQ=%u\n",
                kTouchSclk, kTouchMosi, kTouchMiso, kTouchCs, kTouchIrq);
}

}  // namespace

namespace LvglPort {

void begin() {
  initDisplay();
  initTouch();
  lv_init();

  lv_disp_draw_buf_init(&drawBuffer, drawBufferPixels, nullptr,
                        kScreenWidth * kDrawBufferLines);
  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = kScreenWidth;
  displayDriver.ver_res = kScreenHeight;
  displayDriver.flush_cb = flushDisplay;
  displayDriver.draw_buf = &drawBuffer;
  lv_disp_drv_register(&displayDriver);

  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = readTouch;
  lv_indev_drv_register(&inputDriver);

  Serial.printf("LVGL: %u.%u.%u, draw buffer=%u bytes (%u lines)\n",
                LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                static_cast<unsigned>(kDrawBufferBytes), kDrawBufferLines);
}

void runOnce() { lv_timer_handler(); }

}  // namespace LvglPort
