#include "hal/io.h"

#include <Arduino.h>

#include "config.h"

namespace hal {
namespace {

constexpr uint32_t kDebounceMs = 15;

struct DebouncedInput {
    int      pin;
    bool     stable;
    bool     candidate;
    uint32_t changed_at;

    void begin(int p) {
        pin = p;
        pinMode(pin, kInputsUsePullup ? INPUT_PULLUP : INPUT);
        stable = candidate = read();
        changed_at = millis();
    }

    bool read() const {
        const bool level = digitalRead(pin) == HIGH;
        return kInputsActiveHigh ? level : !level;
    }

    void service(uint32_t now) {
        const bool sample = read();
        if (sample != candidate) {
            candidate  = sample;
            changed_at = now;
        } else if (candidate != stable && (now - changed_at) >= kDebounceMs) {
            stable = candidate;
        }
    }
};

struct BlinkOutput {
    int      pin;
    uint32_t toggle_ms;
    bool     level;
    uint32_t last_toggle;

    void begin(int p) {
        pin = p;
        pinMode(pin, OUTPUT);
        toggle_ms   = static_cast<uint32_t>(Blink::kOff);
        level       = false;
        last_toggle = millis();
        digitalWrite(pin, LOW);
    }

    void set(uint32_t ms) {
        if (ms == toggle_ms) return;
        toggle_ms   = ms;
        last_toggle = millis();
        // Steady states apply immediately; a new cadence starts lit so the
        // change is visible on the first frame.
        level = (ms != static_cast<uint32_t>(Blink::kOff));
        digitalWrite(pin, level ? HIGH : LOW);
    }

    void service(uint32_t now) {
        if (toggle_ms == static_cast<uint32_t>(Blink::kOff) ||
            toggle_ms == static_cast<uint32_t>(Blink::kOn)) {
            return;
        }
        if (now - last_toggle >= toggle_ms) {
            last_toggle = now;
            level       = !level;
            digitalWrite(pin, level ? HIGH : LOW);
        }
    }
};

DebouncedInput g_power;
DebouncedInput g_arm;
DebouncedInput g_fire;

BlinkOutput g_green;
BlinkOutput g_red;

Load g_load = Load::kOpen;

}  // namespace

void ioBegin() {
    g_power.begin(kPinInPowerGood);
    g_arm.begin(kPinInArmStatus);
    g_fire.begin(kPinInFireStatus);

    g_green.begin(kPinOutStatusGreen);
    g_red.begin(kPinOutErrorRed);

#if FEATURE_EXT_LOADS
    pinMode(kPinLoadShort, OUTPUT);
    pinMode(kPinLoadCal, OUTPUT);
    pinMode(kPinLoadDevice, OUTPUT);
#endif
    setLoad(Load::kOpen);
}

bool powerGood()  { return g_power.stable; }
bool armStatus()  { return g_arm.stable; }
bool fireStatus() { return g_fire.stable; }

void setStatusGreen(uint32_t toggle_ms) { g_green.set(toggle_ms); }
void setErrorRed(uint32_t toggle_ms)    { g_red.set(toggle_ms); }

bool statusGreenLit() { return g_green.level; }
bool errorRedLit()    { return g_red.level; }

const char* loadName(Load load) {
    switch (load) {
        case Load::kOpen:      return "Open";
        case Load::kShort:     return "Short";
        case Load::kCalibrate: return "0.22R";
        case Load::kDevice:    return "1R";
    }
    return "?";
}

void setLoad(Load load) {
    g_load = load;

#if FEATURE_EXT_LOADS
    // Break before make: drop every line, then assert the one we want.
    digitalWrite(kPinLoadShort, LOW);
    digitalWrite(kPinLoadCal, LOW);
    digitalWrite(kPinLoadDevice, LOW);
    delayMicroseconds(200);

    switch (load) {
        case Load::kOpen:                                        break;
        case Load::kShort:     digitalWrite(kPinLoadShort, HIGH); break;
        case Load::kCalibrate: digitalWrite(kPinLoadCal, HIGH);   break;
        case Load::kDevice:    digitalWrite(kPinLoadDevice, HIGH); break;
    }
#else
    Serial.printf("[load] select %s (outputs disabled)\n", loadName(load));
#endif
}

Load currentLoad() { return g_load; }

void ioService() {
    const uint32_t now = millis();
    g_power.service(now);
    g_arm.service(now);
    g_fire.service(now);
    g_green.service(now);
    g_red.service(now);
}

}  // namespace hal
