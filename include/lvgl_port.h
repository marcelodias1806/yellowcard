#pragma once

#include <Arduino.h>

namespace LvglPort {

constexpr uint16_t kScreenWidth = 240;
constexpr uint16_t kScreenHeight = 320;
constexpr uint16_t kDrawBufferLines = 20;
constexpr size_t kDrawBufferBytes =
    kScreenWidth * kDrawBufferLines * sizeof(uint16_t);

void begin();
void runOnce();

}  // namespace LvglPort
