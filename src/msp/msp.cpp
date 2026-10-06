#include "msp/msp.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"

namespace msp {
namespace {

constexpr uint32_t kRcFrameMs    = 50;    // RC heartbeat, 20 Hz
constexpr uint32_t kFirePulseMs  = 200;   // fire aux held high for this long
constexpr uint32_t kLinkTimeout  = 500;   // no reply for this long -> link down

#if FEATURE_MSP_UART
HardwareSerial g_uart(1);
#endif

Status   g_status      = {};
bool     g_arm_request = false;
uint32_t g_fire_until  = 0;
uint32_t g_last_rc     = 0;
uint32_t g_last_poll   = 0;

// ---- receive parser ------------------------------------------------------

enum class RxState : uint8_t {
    kIdle, kHeaderM, kHeaderArrow, kSize, kFunction, kPayload, kChecksum
};

RxState g_rx        = RxState::kIdle;
uint8_t g_rx_size   = 0;
uint8_t g_rx_func   = 0;
uint8_t g_rx_count  = 0;
uint8_t g_rx_cksum  = 0;
uint8_t g_rx_buf[64];

void handleStatusPayload(const uint8_t* p, uint8_t len) {
    // MSP_STATUS: cycleTime u16, i2cErrors u16, sensors u16, flightModeFlags
    // u32, configProfile u8.
    if (len < 11) return;

    g_status.cycle_time_us = static_cast<uint16_t>(p[0] | (p[1] << 8));
    g_status.mode_flags    = static_cast<uint32_t>(p[6]) |
                             (static_cast<uint32_t>(p[7]) << 8) |
                             (static_cast<uint32_t>(p[8]) << 16) |
                             (static_cast<uint32_t>(p[9]) << 24);
    // Box 0 is ARM in the standard MultiWii box order.
    g_status.armed = (g_status.mode_flags & 0x01u) != 0;
}

void handleFrame(uint8_t function, const uint8_t* payload, uint8_t len) {
    g_status.frames_rx++;
    g_status.last_reply_ms = millis();
    g_status.link_up       = true;

    if (function == kMspStatus) {
        handleStatusPayload(payload, len);
    }
}

void feed(uint8_t b) {
    switch (g_rx) {
        case RxState::kIdle:
            if (b == '$') g_rx = RxState::kHeaderM;
            break;

        case RxState::kHeaderM:
            // Only v1 replies are parsed; a v2 reply resets the parser rather
            // than being mis-decoded.
            g_rx = (b == 'M') ? RxState::kHeaderArrow : RxState::kIdle;
            break;

        case RxState::kHeaderArrow:
            if (b == '>') {
                g_rx = RxState::kSize;
            } else {
                // '!' is the WH Board rejecting a command.
                if (b == '!') g_status.frames_bad++;
                g_rx = RxState::kIdle;
            }
            break;

        case RxState::kSize:
            g_rx_size  = b;
            g_rx_cksum = b;
            if (g_rx_size > sizeof(g_rx_buf)) {
                g_status.frames_bad++;
                g_rx = RxState::kIdle;
            } else {
                g_rx = RxState::kFunction;
            }
            break;

        case RxState::kFunction:
            g_rx_func   = b;
            g_rx_cksum ^= b;
            g_rx_count  = 0;
            g_rx = (g_rx_size == 0) ? RxState::kChecksum : RxState::kPayload;
            break;

        case RxState::kPayload:
            g_rx_buf[g_rx_count++] = b;
            g_rx_cksum ^= b;
            if (g_rx_count >= g_rx_size) g_rx = RxState::kChecksum;
            break;

        case RxState::kChecksum:
            if (b == g_rx_cksum) {
                handleFrame(g_rx_func, g_rx_buf, g_rx_size);
            } else {
                g_status.frames_bad++;
            }
            g_rx = RxState::kIdle;
            break;
    }
}

// The receive side is always compiled; only the source of bytes changes.
Stream* link() {
#if FEATURE_MSP_UART
    return &g_uart;
#else
    return nullptr;
#endif
}

void logFrame(const uint8_t* frame, size_t len) {
    char line[160];
    size_t n = 0;
    n += snprintf(line + n, sizeof(line) - n, "[msp tx %2u]", (unsigned)len);
    for (size_t i = 0; i < len && n + 4 < sizeof(line); ++i) {
        n += snprintf(line + n, sizeof(line) - n, " %02X", frame[i]);
    }
    Serial.println(line);
}

// RC frames repeat at 20 Hz and status polls at 10 Hz, so the console would be
// unreadable if every frame were logged. Repeating traffic is either silent or
// logged only when its content changes.
enum class LogMode : uint8_t { kAlways, kOnChange, kNever };

void transmit(const uint8_t* frame, size_t len, LogMode mode) {
    if (Stream* s = link()) {
        s->write(frame, len);
    }

    switch (mode) {
        case LogMode::kNever:
            return;

        case LogMode::kOnChange: {
            static uint8_t last[32];
            static size_t  last_len = 0;
            if (len == last_len && memcmp(frame, last, len) == 0) return;
            last_len = (len <= sizeof(last)) ? len : 0;
            if (last_len) memcpy(last, frame, last_len);
            break;
        }

        case LogMode::kAlways:
            break;
    }
    logFrame(frame, len);
}

void sendRcFrame() {
    uint16_t ch[kRcChannelCount];
    for (uint8_t i = 0; i < kRcChannelCount; ++i) ch[i] = kRcCentre;
    ch[0] = ch[1] = ch[3] = kRcCentre;
    ch[2] = kRcLow;  // throttle low

    ch[kAuxArmIndex]  = g_arm_request ? kRcHigh : kRcLow;
    ch[kAuxFireIndex] = (millis() < g_fire_until) ? kRcHigh : kRcLow;

    uint8_t payload[kRcChannelCount * 2];
    for (uint8_t i = 0; i < kRcChannelCount; ++i) {
        payload[i * 2]     = static_cast<uint8_t>(ch[i] & 0xFF);
        payload[i * 2 + 1] = static_cast<uint8_t>(ch[i] >> 8);
    }

    uint8_t frame[sizeof(payload) + 6];
    const size_t len = encodeV1(frame, kMspSetRawRc, payload, sizeof(payload));
    transmit(frame, len, LogMode::kOnChange);
}

void sendStatusRequest() {
    uint8_t frame[6];
    const size_t len = encodeV1(frame, kMspStatus, nullptr, 0);
    transmit(frame, len, LogMode::kNever);
}

}  // namespace

uint8_t crc8DvbS2(uint8_t crc, uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
        crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0xD5)
                           : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}

size_t encodeV1(uint8_t* out, uint8_t function, const uint8_t* payload,
                uint8_t payload_len) {
    size_t n = 0;
    out[n++] = '$';
    out[n++] = 'M';
    out[n++] = '<';
    out[n++] = payload_len;
    out[n++] = function;

    uint8_t cksum = static_cast<uint8_t>(payload_len ^ function);
    for (uint8_t i = 0; i < payload_len; ++i) {
        out[n++] = payload[i];
        cksum   ^= payload[i];
    }
    out[n++] = cksum;
    return n;
}

size_t encodeV2(uint8_t* out, uint16_t function, const uint8_t* payload,
                uint16_t payload_len) {
    size_t n = 0;
    out[n++] = '$';
    out[n++] = 'X';
    out[n++] = '<';

    const uint8_t header[] = {
        0,  // flags
        static_cast<uint8_t>(function & 0xFF),
        static_cast<uint8_t>(function >> 8),
        static_cast<uint8_t>(payload_len & 0xFF),
        static_cast<uint8_t>(payload_len >> 8),
    };

    uint8_t crc = 0;
    for (uint8_t b : header) {
        out[n++] = b;
        crc      = crc8DvbS2(crc, b);
    }
    for (uint16_t i = 0; i < payload_len; ++i) {
        out[n++] = payload[i];
        crc      = crc8DvbS2(crc, payload[i]);
    }
    out[n++] = crc;
    return n;
}

void begin() {
#if FEATURE_MSP_UART
    g_uart.begin(kMspBaud, SERIAL_8N1, kPinMspRx, kPinMspTx);
    Serial.printf("[msp] uart1 up at %u baud (rx=%d tx=%d)\n",
                  (unsigned)kMspBaud, kPinMspRx, kPinMspTx);
#else
    Serial.println("[msp] uart disabled at build time -- frames logged only");
#endif
}

void sendArm(bool armed) {
    g_arm_request = armed;
    if (!armed) g_fire_until = 0;  // disarm cancels any in-flight fire pulse
    sendRcFrame();
    Serial.printf("[msp] arm request = %d\n", armed ? 1 : 0);
}

void sendFire() {
    if (!g_arm_request) {
        Serial.println("[msp] fire suppressed -- not armed");
        return;
    }
    g_fire_until = millis() + kFirePulseMs;
    sendRcFrame();
    Serial.println("[msp] fire pulse sent");
}

bool armRequested() { return g_arm_request; }

Status status() { return g_status; }

void service() {
    const uint32_t now = millis();

    if (Stream* s = link()) {
        while (s->available()) feed(static_cast<uint8_t>(s->read()));
    }

    if (now - g_last_rc >= kRcFrameMs) {
        g_last_rc = now;
        sendRcFrame();
    }

    if (now - g_last_poll >= kMspPollMs) {
        g_last_poll = now;
        sendStatusRequest();
    }

    if (g_status.link_up && (now - g_status.last_reply_ms) > kLinkTimeout) {
        g_status.link_up = false;
    }
}

}  // namespace msp
