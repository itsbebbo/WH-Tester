// LVGL <-> M5GFX bridge: flush callback, capacitive touch input, tick source.
#pragma once

namespace hal {

// Brings up M5Unified, LVGL, the display driver and the touch input device.
// Must be called once from setup() before any lv_* call.
void displayBegin();

// Feeds LVGL its tick and runs the timer handler. Call every loop iteration.
void displayService();

}  // namespace hal
