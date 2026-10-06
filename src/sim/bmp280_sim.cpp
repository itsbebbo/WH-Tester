#include "sim/bmp280_sim.h"

#include <Arduino.h>

#include "config.h"

#if FEATURE_BMP280_SIM
#include <Wire.h>
#endif

namespace sim {
namespace {

Altitude g_altitude = Altitude::kGround;

#if FEATURE_BMP280_SIM
// Register map the WH Board will read. Only the pieces a stock BMP280 driver
// touches are modelled: chip ID, calibration block, control/config, and the
// burst-readable pressure/temperature registers.
constexpr uint8_t kRegCalib00 = 0x88;
constexpr uint8_t kRegChipId  = 0xD0;
constexpr uint8_t kRegReset   = 0xE0;
constexpr uint8_t kRegStatus  = 0xF3;
constexpr uint8_t kRegCtrlMeas = 0xF4;
constexpr uint8_t kRegConfig  = 0xF5;
constexpr uint8_t kRegPressMsb = 0xF7;

constexpr uint8_t kChipIdBmp280 = 0x58;

// OPEN ITEM: the calibration block below is a placeholder. A real BMP280
// driver runs the Bosch compensation polynomial over dig_T1..dig_P9, so the
// raw registers we publish must be the *pre-compensation* values that yield
// the target pressure for whatever coefficients we advertise. Deriving that
// pair (either by inverting the polynomial, or by copying a real sensor's
// coefficients and inverting against those) is required before this simulator
// reads correctly at the WH Board. Everything else here -- the register map,
// the pointer auto-increment, the I2C slave plumbing -- is complete.
volatile uint8_t g_regs[256];
volatile uint8_t g_reg_pointer = 0;

void writeRaw(uint8_t reg, uint32_t raw20) {
    g_regs[reg]     = static_cast<uint8_t>((raw20 >> 12) & 0xFF);
    g_regs[reg + 1] = static_cast<uint8_t>((raw20 >> 4) & 0xFF);
    g_regs[reg + 2] = static_cast<uint8_t>((raw20 << 4) & 0xF0);
}

void onReceive(int count) {
    if (count <= 0) return;
    g_reg_pointer = static_cast<uint8_t>(Wire1.read());
    while (Wire1.available()) {
        const uint8_t value = static_cast<uint8_t>(Wire1.read());
        if (g_reg_pointer == kRegReset) continue;  // reset is a no-op here
        g_regs[g_reg_pointer++] = value;
    }
}

void onRequest() {
    // The BMP280 auto-increments on burst reads.
    uint8_t buf[32];
    for (uint8_t i = 0; i < sizeof(buf); ++i) {
        buf[i] = g_regs[static_cast<uint8_t>(g_reg_pointer + i)];
    }
    Wire1.write(buf, sizeof(buf));
}
#endif

}  // namespace

void bmp280Begin() {
#if FEATURE_BMP280_SIM
    for (uint16_t i = 0; i < 256; ++i) g_regs[i] = 0;

    g_regs[kRegChipId]   = kChipIdBmp280;
    g_regs[kRegStatus]   = 0x00;   // not measuring, not copying
    g_regs[kRegCtrlMeas] = 0x57;   // normal mode, x16 oversampling
    g_regs[kRegConfig]   = 0x00;

    // Placeholder coefficients -- see the OPEN ITEM note above.
    g_regs[kRegCalib00]     = 0x01;
    g_regs[kRegCalib00 + 1] = 0x00;
    g_regs[kRegCalib00 + 6] = 0x01;
    g_regs[kRegCalib00 + 7] = 0x00;

    Wire1.begin(kBmp280Address, kPinBmpSda, kPinBmpScl, 400000);
    Wire1.onReceive(onReceive);
    Wire1.onRequest(onRequest);
    Serial.printf("[bmp280] slave at 0x%02X (sda=%d scl=%d)\n",
                  kBmp280Address, kPinBmpSda, kPinBmpScl);
#else
    Serial.println("[bmp280] simulator disabled at build time");
#endif
    setPressure(Altitude::kGround);
}

void setPressure(Altitude altitude) {
    g_altitude = altitude;
    Serial.printf("[bmp280] %s (%.0f Pa)\n",
                  altitude == Altitude::kAir ? "air/1000ft" : "ground/sea level",
                  pressurePa());
#if FEATURE_BMP280_SIM
    bmp280Service();
#endif
}

Altitude pressure() { return g_altitude; }

float pressurePa() {
    return g_altitude == Altitude::kAir ? kPressureAirPa : kPressureGroundPa;
}

void bmp280Service() {
#if FEATURE_BMP280_SIM
    // Publishes the target pressure directly into the raw registers. Correct
    // only once the calibration coefficients above are resolved.
    writeRaw(kRegPressMsb, static_cast<uint32_t>(pressurePa() * 256.0f) & 0xFFFFFu);
    writeRaw(kRegPressMsb + 3,
             static_cast<uint32_t>((kSimTemperatureC * 100.0f) * 16.0f) & 0xFFFFFu);
#endif
}

}  // namespace sim
