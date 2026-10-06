#include "hal/audio.h"

#include <M5Unified.h>

#include "assets/boom_pcm.h"
#include "config.h"

namespace hal {

void audioBegin() {
    auto spk = M5.Speaker.config();
    spk.sample_rate = kBoomPcm_kSampleRate;
    M5.Speaker.config(spk);
    M5.Speaker.begin();
    M5.Speaker.setVolume(kAlertVolume);
}

void playBoom() {
    // playRaw(data, samples, rate, stereo, repeat, channel, stop_current)
    M5.Speaker.playRaw(kBoomPcm, kBoomPcm_kSampleCount, kBoomPcm_kSampleRate,
                       false, 1, -1, true);
}

}  // namespace hal
