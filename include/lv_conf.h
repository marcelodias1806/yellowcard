#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* YellowCard F1: RGB565, partial rendering, no full-screen framebuffer. */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Keep LVGL's internal allocator bounded on the ESP32 without PSRAM. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (32U * 1024U)

/* Arduino millis() supplies the LVGL tick; no timer task is required. */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Local, 1-bit QR generation provided by LVGL 8.x. */
#define LV_USE_CANVAS 1
#define LV_USE_QRCODE 1

#endif  // LV_CONF_H
