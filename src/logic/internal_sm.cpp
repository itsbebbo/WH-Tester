#include "logic/internal_sm.h"

#include <Arduino.h>

#include "config.h"
#include "hal/io.h"

namespace logic {
namespace {

InternalState g_state       = InternalState::kIdle;
ErrorCause    g_error       = ErrorCause::kNone;
ErrorCause    g_entry_fault = ErrorCause::kNone;
uint32_t      g_arm_started = 0;
bool          g_fire_event  = false;

// Evaluates the invalid-combination rules. Returns kNone when the inputs are
// consistent.
ErrorCause classify(bool power, bool arm, bool fire) {
    if (fire && !power) return ErrorCause::kFireWithoutPower;
    if (arm && !power)  return ErrorCause::kArmWithoutPower;
    if (fire && !arm)   return ErrorCause::kFireWithoutArm;
    return ErrorCause::kNone;
}

void applyOutputs() {
    switch (g_state) {
        case InternalState::kError:
            hal::setErrorRed(kBlinkErrorMs);
            hal::setStatusGreen(kBlinkErrorMs);
            break;

        case InternalState::kIdle:
            hal::setErrorRed(static_cast<uint32_t>(hal::Blink::kOff));
            hal::setStatusGreen(static_cast<uint32_t>(hal::Blink::kOff));
            break;

        case InternalState::kPower:
            hal::setErrorRed(static_cast<uint32_t>(hal::Blink::kOff));
            hal::setStatusGreen(kBlinkPowerMs);
            break;

        case InternalState::kArming:
            hal::setErrorRed(static_cast<uint32_t>(hal::Blink::kOff));
            hal::setStatusGreen(kBlinkArmingMs);
            break;

        case InternalState::kArmed:
            hal::setErrorRed(static_cast<uint32_t>(hal::Blink::kOff));
            hal::setStatusGreen(kBlinkArmedMs);
            break;

        case InternalState::kFired:
            // Not a cadence the spec defines: steady green reads as clearly
            // distinct from every blink rate, so a fire cannot be mistaken for
            // one of the other states.
            hal::setErrorRed(static_cast<uint32_t>(hal::Blink::kOff));
            hal::setStatusGreen(static_cast<uint32_t>(hal::Blink::kOn));
            break;
    }
}

}  // namespace

void internalBegin() { internalReset(); }

void internalReset() {
    g_state       = InternalState::kIdle;
    g_arm_started = 0;
    g_fire_event  = false;

    // Entry gate: the tester has commanded nothing yet, so finding arm or fire
    // already asserted means the WH Board is not in a known-safe starting
    // state. Latched here and cleared in internalService() once both are low.
    if (hal::fireStatus()) {
        g_entry_fault = ErrorCause::kFireActiveOnEntry;
    } else if (hal::armStatus()) {
        g_entry_fault = ErrorCause::kArmActiveOnEntry;
    } else {
        g_entry_fault = ErrorCause::kNone;
    }

    g_error = g_entry_fault;
    if (g_error != ErrorCause::kNone) g_state = InternalState::kError;

    applyOutputs();
}

void internalService() {
    const bool power = hal::powerGood();
    const bool arm   = hal::armStatus();
    const bool fire  = hal::fireStatus();
    const uint32_t now = millis();

    const InternalState previous = g_state;

    // A latched entry fault clears only once the WH Board drops both lines.
    if (g_entry_fault != ErrorCause::kNone && !arm && !fire) {
        g_entry_fault = ErrorCause::kNone;
    }

    // A live invalid combination outranks the latched entry fault, so the
    // operator sees the more specific cause.
    g_error = classify(power, arm, fire);
    if (g_error == ErrorCause::kNone) g_error = g_entry_fault;

    if (g_error != ErrorCause::kNone) {
        g_state = InternalState::kError;
    } else if (!power) {
        g_state = InternalState::kIdle;
    } else if (!arm) {
        g_state = InternalState::kPower;
    } else {
        // Arm is asserted and the inputs are consistent. The settle window
        // starts when the arm path is first entered and keeps running across a
        // fire, so a fire does not restart the arming clock.
        const bool in_arm_path = previous == InternalState::kArming ||
                                 previous == InternalState::kArmed ||
                                 previous == InternalState::kFired;
        if (!in_arm_path) g_arm_started = now;

        if (fire) {
            // Fire following arming is a valid fire: act on it immediately.
            g_state = InternalState::kFired;
        } else if ((now - g_arm_started) >= kArmingSettleMs) {
            g_state = InternalState::kArmed;
        } else {
            g_state = InternalState::kArming;
        }
    }

    // The alert is raised on entry to kFired, i.e. only for a fire that
    // followed arming. A fire without arm is a fault and reports as one.
    if (g_state == InternalState::kFired && previous != InternalState::kFired) {
        g_fire_event = true;
    }

    if (g_state != previous) applyOutputs();
}

InternalState internalState() { return g_state; }
ErrorCause    internalError() { return g_error; }

uint32_t armingRemainingMs() {
    if (g_state != InternalState::kArming) return 0;
    const uint32_t elapsed = millis() - g_arm_started;
    return (elapsed >= kArmingSettleMs) ? 0 : (kArmingSettleMs - elapsed);
}

bool takeFireEvent() {
    const bool event = g_fire_event;
    g_fire_event = false;
    return event;
}

const char* internalStateName() {
    switch (g_state) {
        case InternalState::kIdle:   return "NO POWER";
        case InternalState::kPower:  return "POWERED";
        case InternalState::kArming: return "ARMING";
        case InternalState::kArmed:  return "ARMED";
        case InternalState::kFired:  return "FIRED";
        case InternalState::kError:  return "ERROR";
    }
    return "?";
}

const char* internalErrorText() {
    switch (g_error) {
        case ErrorCause::kNone:             return "";
        case ErrorCause::kFireWithoutArm:   return "FIRE without ARM";
        case ErrorCause::kFireWithoutPower: return "FIRE without POWER";
        case ErrorCause::kArmWithoutPower:  return "ARM without POWER";
        case ErrorCause::kFireActiveOnEntry: return "FIRE active on entry";
        case ErrorCause::kArmActiveOnEntry:  return "ARM active on entry";
    }
    return "";
}

}  // namespace logic
