// INA219 current sensor on the CoreS3 internal I2C bus (GPIO 12/11), used as
// the external-mode observation path.
#pragma once

#include <stdint.h>

namespace hal {

struct CurrentReading {
    float    current_ma;
    bool     valid;       // false until the sensor has answered at least once
    uint32_t sample_ms;   // millis() of the sample
};

void currentSensorBegin();

// True while the INA219 is acknowledging on the bus.
bool currentSensorPresent();

// Latest sample. Fields are zero and valid==false when absent.
CurrentReading currentReading();

// Above the spec threshold (1200 mA) with hysteresis applied.
bool outputActive();

// Zero offset captured with no load connected, subtracted from later readings.
// Applied to the current value; not surfaced in the UI.
void captureZeroOffset();

void currentSensorService();

}  // namespace hal
