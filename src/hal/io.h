// Digital I/O to the WH Board: three status inputs, two indicator
// outputs, and the external-mode load-select lines.
#pragma once

#include <stdint.h>

namespace hal {

// Blink cadence for an indicator output. kOff/kOn are steady states; any other
// value is the toggle interval in milliseconds.
enum class Blink : uint32_t { kOff = 0, kOn = 0xFFFFFFFFu };

// Which load the external-mode selector has applied to the WH Board output.
enum class Load : uint8_t {
    kOpen = 0,   // nothing connected
    kShort,      // dead short -- short-circuit error case
    kCalibrate,  // 0.22 ohm calibration load
    kDevice,     // 1 ohm simulated device
};

void ioBegin();

// Debounced WH Board status inputs, already corrected for kInputsActiveHigh.
bool powerGood();
bool armStatus();
bool fireStatus();

// Indicator outputs. Pass a cadence in ms, or Blink::kOff / Blink::kOn.
void setStatusGreen(uint32_t toggle_ms);
void setErrorRed(uint32_t toggle_ms);

// Current logical level of each output, so the UI can mirror the real pin
// rather than re-deriving the blink phase.
bool statusGreenLit();
bool errorRedLit();

// External-mode load selection. A no-op that only logs while
// FEATURE_EXT_LOADS is 0.
void setLoad(Load load);
Load currentLoad();
const char* loadName(Load load);

// Samples inputs and advances blink phases. Call every loop iteration.
void ioService();

}  // namespace hal
