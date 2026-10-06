// Internal-mode state machine.
//
// Drives the tester's own two indicator outputs (green status on GPIO8, red
// error on GPIO9) from the three WH Board status inputs, using the cadences
// given in specification section 6:
//
//   error   -> red and green both toggle at 100 ms
//   power   -> green toggles at 166 ms
//   arming  -> green toggles at 1000 ms for 10 s after the arm signal
//   armed   -> green toggles at 600 ms
//
// An error is latched when the WH Board asserts fire without arm, or asserts
// arm or fire without power. It clears when the offending inputs go away.
//
// Entering the screen with arm or fire already asserted is also an error: the
// tester has not commanded either, so the WH Board is not in a known-safe
// starting state. That fault is latched at entry and clears only once both
// lines are low again.
//
// A fire input received while armed is a valid fire and moves the machine to
// kFired immediately, which is what raises the alert.
#pragma once

#include <stdint.h>

namespace logic {

enum class InternalState : uint8_t {
    kIdle,    // no power from the WH Board
    kPower,   // power good, not armed
    kArming,  // arm asserted, inside the settling window
    kArmed,   // arm asserted, settled
    kFired,   // fire status asserted while armed -- a valid fire
    kError,   // invalid input combination
};

enum class ErrorCause : uint8_t {
    kNone = 0,
    kFireWithoutArm,
    kFireWithoutPower,
    kArmWithoutPower,
    kFireActiveOnEntry,  // WH Board already firing when the screen was entered
    kArmActiveOnEntry,   // WH Board already armed when the screen was entered
};

void internalBegin();

// Samples the inputs and updates the outputs. Call every loop iteration.
void internalService();

// Starts a fresh session: clears state and re-evaluates the entry condition
// above. Called on entering the Internal screen.
void internalReset();

InternalState internalState();
ErrorCause    internalError();
const char*   internalStateName();
const char*   internalErrorText();

// Milliseconds remaining in the arming settle window, 0 outside it.
uint32_t armingRemainingMs();

// True on the transition into a WH Board fire-status assertion, consumed once
// by the caller so the alert fires exactly one time per event.
bool takeFireEvent();

}  // namespace logic
