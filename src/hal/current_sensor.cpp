#include "hal/current_sensor.h"

#include <Arduino.h>
#include <M5Unified.h>

#include "config.h"

namespace hal {
namespace {

bool           g_present = false;

#if FEATURE_INA219
constexpr uint32_t kProbeRetryMs = 2000;
uint32_t       g_last_probe = 0;
#endif
CurrentReading g_reading = {};
bool           g_active  = false;
float          g_zero_ma = 0.0f;
uint32_t       g_last_sample = 0;

}  // namespace

namespace {

#if FEATURE_INA219
// The INA219 shares the CoreS3's internal I2C bus (GPIO 12 SDA / 11 SCL) with
// the PMU, touch controller, IMU and audio codecs. M5Unified owns that bus
// through its own driver, so the sensor is talked to through M5.In_I2C -- which
// serialises access with the rest of M5Unified -- rather than a second Arduino
// Wire instance fighting over the same pins. That rules out Adafruit_INA219,
// which only accepts a TwoWire, so the handful of registers used are driven
// directly below.

constexpr uint32_t kI2cFreq = 400000;

constexpr uint8_t kRegConfig      = 0x00;
constexpr uint8_t kRegCurrent     = 0x04;
constexpr uint8_t kRegCalibration = 0x05;

// Identical to Adafruit_INA219::setCalibration_32V_2A(): 32 V bus range,
// /8 gain (320 mV shunt), 12-bit ADCs, continuous shunt + bus conversion.
// With the 0.1 ohm shunt that gives a 100 uA current LSB, so the current
// register divided by 10 is milliamps. The spec's active threshold is
// 1200 mA, so this is the only default range that reaches it -- the finer
// ranges top out at 1 A or 400 mA.
constexpr uint16_t kConfig32V2A     = 0x399F;
constexpr uint16_t kCalibration32V2A = 4096;
constexpr float    kCurrentDivider   = 10.0f;

bool writeRegister16(uint8_t reg, uint16_t value) {
    const uint8_t buf[2] = {static_cast<uint8_t>(value >> 8),
                            static_cast<uint8_t>(value & 0xFF)};
    return M5.In_I2C.writeRegister(kIna219Address, reg, buf, sizeof(buf), kI2cFreq);
}

bool readRegister16(uint8_t reg, uint16_t& value) {
    uint8_t buf[2];
    if (!M5.In_I2C.readRegister(kIna219Address, reg, buf, sizeof(buf), kI2cFreq)) {
        return false;
    }
    value = static_cast<uint16_t>((buf[0] << 8) | buf[1]);
    return true;
}

// Probes the address and applies the 32V/2A configuration. False if the chip
// did not acknowledge.
bool configure() {
    if (!M5.In_I2C.scanID(kIna219Address, kI2cFreq)) return false;
    return writeRegister16(kRegCalibration, kCalibration32V2A) &&
           writeRegister16(kRegConfig, kConfig32V2A);
}

// Reports every address that ACKs, so a wiring or address fault is visible on
// the console instead of just "not detected". On this bus the CoreS3's own
// peripherals always appear too.
void scanBus() {
    Serial.print("[i2c] internal bus scan:");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        if (M5.In_I2C.scanID(addr, kI2cFreq)) {
            Serial.printf(" 0x%02X", addr);
            ++found;
        }
    }
    if (found == 0) Serial.print(" (nothing responded)");
    Serial.println();
}
#endif

}  // namespace

void currentSensorBegin() {
#if FEATURE_INA219
    Serial.printf("[ina219] internal I2C: sda=%d scl=%d, address 0x%02X\n",
                  M5.In_I2C.getSDA(), M5.In_I2C.getSCL(), kIna219Address);
    scanBus();

    g_present = configure();
    if (g_present) {
        Serial.println("[ina219] ready, 32V/2A calibration");
    } else {
        Serial.printf("[ina219] not found at 0x%02X -- check the GPIO 11/12 "
                      "wiring and the A0/A1 address jumpers\n", kIna219Address);
    }
#else
    Serial.println("[ina219] disabled at build time");
#endif
}

bool currentSensorPresent() { return g_present; }

CurrentReading currentReading() { return g_reading; }

bool outputActive() { return g_active; }

void captureZeroOffset() {
    if (!g_present) return;
    // The stored reading is already offset-corrected, so adding the old
    // offset back recovers the raw value -- which is the new offset.
    g_zero_ma = g_reading.current_ma + g_zero_ma;
    Serial.printf("[ina219] zero offset captured: %.2f mA\n", g_zero_ma);
}

void currentSensorService() {
    const uint32_t now = millis();

    // Re-probe periodically so a sensor plugged in after boot is picked up
    // without a reset -- otherwise the screen reads NO SENSOR forever.
    if (!g_present) {
#if FEATURE_INA219
        if (now - g_last_probe < kProbeRetryMs) return;
        g_last_probe = now;

        if (configure()) {
            g_present = true;
            Serial.println("[ina219] appeared on the bus, now live");
        }
#endif
        return;
    }

    if (now - g_last_sample < kIna219SampleMs) return;
    g_last_sample = now;

#if FEATURE_INA219
    // Current is the only quantity this tester measures, so only the current
    // register is read. The calibration register is rewritten first, as the
    // Adafruit driver does, so a brown-out reset of the chip cannot leave it
    // reading zero.
    uint16_t raw = 0;
    if (!writeRegister16(kRegCalibration, kCalibration32V2A) ||
        !readRegister16(kRegCurrent, raw)) {
        g_present       = false;
        g_reading.valid = false;
        g_active        = false;
        Serial.println("[ina219] stopped responding, re-probing");
        return;
    }

    g_reading.current_ma = static_cast<int16_t>(raw) / kCurrentDivider - g_zero_ma;
    g_reading.valid      = true;
    g_reading.sample_ms  = now;

    // Hysteresis around the threshold so a reading sitting on 1200 mA cannot
    // chatter the fire indicator.
    const float rise = kFireCurrentThresholdMa + kFireCurrentHysteresisMa;
    const float fall = kFireCurrentThresholdMa - kFireCurrentHysteresisMa;
    if (!g_active && g_reading.current_ma >= rise) {
        g_active = true;
    } else if (g_active && g_reading.current_ma <= fall) {
        g_active = false;
    }
#endif
}

}  // namespace hal
