// BMP280 pressure simulator.
//
// The tester presents itself to the WH Board as a BMP280 on a secondary I2C
// bus, so the WH Board's own barometer driver reads whichever altitude the
// operator has selected. Behind FEATURE_BMP280_SIM: with the flag at 0 the
// selection is tracked and logged but no bus is claimed.
#pragma once

#include <stdint.h>

namespace sim {

enum class Altitude : uint8_t { kGround = 0, kAir = 1 };

void bmp280Begin();

void setPressure(Altitude altitude);
Altitude pressure();

// Pressure in pascals for the current selection.
float pressurePa();

void bmp280Service();

}  // namespace sim
