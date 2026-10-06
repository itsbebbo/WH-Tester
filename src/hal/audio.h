// Onboard speaker (AW88298) playback for the fire alert.
#pragma once

namespace hal {

void audioBegin();

// Plays the baked-in explosion sample. Non-blocking: M5Unified streams the
// buffer from the I2S task.
void playBoom();

}  // namespace hal
