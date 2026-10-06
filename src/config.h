// Single source of truth for pins, timings, thresholds and feature flags.
// Nothing else in the firmware should hard-code a pin number or a magic time.
#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// Feature flags
// ---------------------------------------------------------------------------
// Subsystems that are specified but not yet wired on the bench. When a flag is
// 0 the UI still behaves exactly as it will with the hardware present -- the
// control is live, the state machine runs -- but the hardware call is replaced
// by a log line on the USB console. Flip the flag to 1 once the wiring exists.

#define FEATURE_MSP_UART      0  // MSP serial link, Grove C. Disabled.
#define FEATURE_BMP280_SIM    0  // I2C slave pressure simulator. Disabled.
#define FEATURE_EXT_LOADS     0  // External load-select outputs. Disabled.
#define FEATURE_INA219        1  // Current sensor, internal I2C (GPIO 11/12).

// ---------------------------------------------------------------------------
// Internal-mode I/O  (pins per specification section 6)
// ---------------------------------------------------------------------------
// Inputs are WH Board status lines. They break out on the M-Bus, so a base
// module is required. Active level is configurable because the WH Board drive
// polarity is not yet confirmed.

constexpr int  kPinInPowerGood   = 5;   // WH Board power-on status
constexpr int  kPinInArmStatus   = 6;   // WH Board arm status
constexpr int  kPinInFireStatus  = 7;   // WH Board fire status
constexpr bool kInputsActiveHigh = true;
constexpr bool kInputsUsePullup  = true;  // internal pull-up; set false if the
                                          // WH Board drives both rails

// Outputs driven by the internal state machine. Both are on Grove Port B.
constexpr int kPinOutStatusGreen = 8;   // Grove B -- green status
constexpr int kPinOutErrorRed    = 9;   // Grove B -- red error

// ---------------------------------------------------------------------------
// Internal state-machine blink cadences (spec section 6)
// ---------------------------------------------------------------------------
// Values are the toggle interval, i.e. a full cycle is twice this.

constexpr uint32_t kBlinkErrorMs   = 100;    // error: red and green together
constexpr uint32_t kBlinkPowerMs   = 166;    // power good, not yet armed
constexpr uint32_t kBlinkArmingMs  = 1000;   // arm received, settling
constexpr uint32_t kBlinkArmedMs   = 600;    // fully armed
constexpr uint32_t kArmingSettleMs = 10000;  // arming -> armed dwell

// ---------------------------------------------------------------------------
// External-mode load selection  (FEATURE_EXT_LOADS)
// ---------------------------------------------------------------------------
// One active-high line per load. Exactly one is asserted at a time; Open
// Circuit asserts none. Pins are placeholders until the load board exists --
// they are only touched when FEATURE_EXT_LOADS is 1.

// GPIO 13 must NOT be used here: it is the CoreS3's I2S speaker data line
// (M5Unified spk_cfg.pin_data_out), and driving it fights the fire-alert audio.
constexpr int kPinLoadShort   = 10;  // dead short across the WH Board output
constexpr int kPinLoadCal     = 14;  // 0.22 ohm calibration load
constexpr int kPinLoadDevice  = 16;  // 1 ohm simulated device

// ---------------------------------------------------------------------------
// INA219 current sensor (CoreS3 internal I2C, GPIO 12 SDA / 11 SCL)
// ---------------------------------------------------------------------------
// Shares the bus M5Unified already owns, so there are no pins to assign here.
// The address must NOT be the INA219 default 0x40: that is the CoreS3's ES7210
// microphone codec. 0x41 is the Adafruit breakout with the A0 jumper bridged.
constexpr uint8_t  kIna219Address    = 0x41;
constexpr uint32_t kIna219SampleMs   = 100;

// Spec section 1: above this the WH Board output counts as a correct active
// output.
constexpr float kFireCurrentThresholdMa = 1200.0f;

// Hysteresis, so a reading sitting on the threshold cannot chatter the
// indicator. Rises through +hyst, falls through -hyst.
constexpr float kFireCurrentHysteresisMa = 25.0f;

// ---------------------------------------------------------------------------
// MSP serial link to the WH Board (Grove Port C, FEATURE_MSP_UART)
// ---------------------------------------------------------------------------
constexpr int      kPinMspTx   = 17;  // Grove C
constexpr int      kPinMspRx   = 18;  // Grove C
constexpr uint32_t kMspBaud    = 115200;
constexpr uint32_t kMspPollMs  = 100;  // MSP_STATUS poll interval

// ---------------------------------------------------------------------------
// BMP280 pressure simulator (FEATURE_BMP280_SIM)
// ---------------------------------------------------------------------------
// The CoreS3 acts as an I2C *slave* at the BMP280 address so the WH Board's own
// barometer driver reads the pressure we choose.
// UNASSIGNED. GPIO 10/14/16 are the last free pins on the CoreS3 and the load
// selector above now claims all three, so the simulator has no pins left on the
// bare board -- see the pin-budget note in README.md. Enabling
// FEATURE_BMP280_SIM without assigning real pins is a compile error, by design.
constexpr int     kPinBmpSda     = -1;
constexpr int     kPinBmpScl     = -1;
constexpr uint8_t kBmp280Address = 0x76;

constexpr float kPressureGroundPa = 101325.0f;  // sea level, ISA
constexpr float kPressureAirPa    = 97717.0f;   // 1000 ft, ISA
constexpr float kSimTemperatureC  = 20.0f;

// ---------------------------------------------------------------------------
// Fire alert (spec section 9)
// ---------------------------------------------------------------------------
constexpr uint32_t kFlashDurationMs = 200;
constexpr uint8_t  kAlertVolume     = 200;  // 0-255
constexpr uint32_t kAlertLockoutMs  = 1500; // ignore re-triggers within this

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
constexpr uint32_t kUiTickMs = 5;    // LVGL tick / task period

// ---------------------------------------------------------------------------
// Pin-map guards
// ---------------------------------------------------------------------------
// Pins the CoreS3 claims for itself. Assigning one of these silently fights a
// peripheral -- GPIO 13 (speaker data) was doing exactly that.

namespace pinmap {

constexpr int kReserved[] = {
    3, 4,        // LCD CS, TF card CS
    11, 12,      // internal I2C (PMU, touch, IMU, AW9523)
    13, 33, 34,  // I2S speaker: data / WS / BCK
    35, 36, 37,  // M-Bus SPI
    43, 44,      // USB console UART
};

constexpr int kReservedCount = sizeof(kReserved) / sizeof(kReserved[0]);

constexpr bool isReserved(int pin, int i = 0) {
    return i >= kReservedCount ? false
         : (kReserved[i] == pin ? true : isReserved(pin, i + 1));
}

// Every pin this firmware drives or reads, in one list.
constexpr int kUsed[] = {
    kPinInPowerGood, kPinInArmStatus, kPinInFireStatus,
    kPinOutStatusGreen, kPinOutErrorRed,
    kPinLoadShort, kPinLoadCal, kPinLoadDevice,
    kPinMspTx, kPinMspRx,
};

constexpr int kUsedCount = sizeof(kUsed) / sizeof(kUsed[0]);

constexpr int occurrences(int pin, int i = 0) {
    return i >= kUsedCount ? 0
         : ((kUsed[i] == pin ? 1 : 0) + occurrences(pin, i + 1));
}

// True if any entry appears more than once.
constexpr bool hasDuplicate(int i = 0) {
    return i >= kUsedCount ? false
         : (occurrences(kUsed[i]) > 1 ? true : hasDuplicate(i + 1));
}

constexpr bool anyReserved(int i = 0) {
    return i >= kUsedCount ? false
         : (isReserved(kUsed[i]) ? true : anyReserved(i + 1));
}

// I2C addresses already taken on the CoreS3 internal bus, which the INA219 now
// shares: magnetometer, camera, light/proximity, PMU, AW88298 amp, touch,
// ES7210 mic codec, RTC, AW9523 IO expander, IMU.
constexpr uint8_t kInternalI2cAddrs[] = {
    0x10, 0x21, 0x23, 0x34, 0x36, 0x38, 0x40, 0x51, 0x58, 0x69,
};

constexpr int kInternalI2cAddrCount =
    sizeof(kInternalI2cAddrs) / sizeof(kInternalI2cAddrs[0]);

constexpr bool isInternalI2cAddr(uint8_t addr, int i = 0) {
    return i >= kInternalI2cAddrCount ? false
         : (kInternalI2cAddrs[i] == addr ? true : isInternalI2cAddr(addr, i + 1));
}

}  // namespace pinmap

static_assert(!pinmap::isInternalI2cAddr(kIna219Address),
              "config.h: kIna219Address collides with a CoreS3 internal I2C "
              "device -- 0x40 is the ES7210 mic codec; use 0x41-0x4F");

static_assert(!pinmap::hasDuplicate(),
              "config.h: two functions are assigned the same GPIO");
static_assert(!pinmap::anyReserved(),
              "config.h: a GPIO reserved by the CoreS3 has been assigned "
              "(speaker/LCD/SD/internal-I2C/SPI/USB) -- see pinmap::kReserved");

#if FEATURE_BMP280_SIM
static_assert(kPinBmpSda >= 0 && kPinBmpScl >= 0,
              "FEATURE_BMP280_SIM needs real pins: the CoreS3 has none free "
              "once the load selector is wired, so a base module is required");
static_assert(!pinmap::isReserved(kPinBmpSda) && !pinmap::isReserved(kPinBmpScl),
              "BMP280 simulator pins collide with a CoreS3 reserved pin");
#endif
