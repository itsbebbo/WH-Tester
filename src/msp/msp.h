// MultiWii Serial Protocol link to the WH Board.
//
// Implements MSP v1 ($M) framing with the standard XOR checksum, plus MSP v2
// ($X) with CRC8/DVB-S2 for function IDs above 255. The transport sits behind
// FEATURE_MSP_UART: with the flag at 0 every frame is logged to the USB
// console instead of being written to Grove Port C, so the UI and state
// machine can be exercised on the bench with no WH Board attached.
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace msp {

// Standard MultiWii function IDs used here.
enum : uint16_t {
    kMspStatus     = 101,  // WH Board -> tester: mode flags, cycle time
    kMspSetRawRc   = 200,  // tester -> WH Board: RC channel values
};

// ARM and FIRE are carried as RC aux channels, which is how MSP arms a
// MultiWii/Betaflight-style target. If the WH Board instead exposes dedicated
// MSP function IDs, change these three constants and sendArm()/sendFire() --
// nothing else in the firmware knows how a command is framed.
constexpr uint8_t  kRcChannelCount = 8;
constexpr uint8_t  kAuxArmIndex    = 4;   // AUX1
constexpr uint8_t  kAuxFireIndex   = 5;   // AUX2
constexpr uint16_t kRcLow          = 1000;
constexpr uint16_t kRcHigh         = 1800;
constexpr uint16_t kRcCentre       = 1500;

// Decoded WH Board state, refreshed by MSP_STATUS replies.
struct Status {
    bool     armed;
    bool     link_up;      // a well-formed reply arrived recently
    uint32_t last_reply_ms;
    uint16_t cycle_time_us;
    uint32_t mode_flags;
    uint32_t frames_rx;
    uint32_t frames_bad;
};

void begin();

// Latch the requested arm state and push it to the WH Board. The arm channel is
// held at its level by every subsequent RC frame -- MSP arming is a level,
// not an edge.
void sendArm(bool armed);

// One momentary fire pulse on the fire aux channel.
void sendFire();

bool armRequested();

Status status();

// Pumps the RC frame heartbeat, the status poll and the receive parser.
void service();

// ---- Framing primitives, exposed for testing ----------------------------

// Encodes an MSP v1 request into `out` (which must hold payload_len + 6
// bytes). Returns the frame length.
size_t encodeV1(uint8_t* out, uint8_t function, const uint8_t* payload,
                uint8_t payload_len);

// Encodes an MSP v2 request into `out` (which must hold payload_len + 9
// bytes). Returns the frame length.
size_t encodeV2(uint8_t* out, uint16_t function, const uint8_t* payload,
                uint16_t payload_len);

uint8_t crc8DvbS2(uint8_t crc, uint8_t byte);

}  // namespace msp
