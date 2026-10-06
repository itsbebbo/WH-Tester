# Arm/Fire Circuit Functional Tester — M5Stack CoreS3

Firmware implementing `CoreS3_Test_Unit_Specification.md`. The tester commands a
WH Board over MSP and independently observes its outputs: three
digital status lines in **Internal** mode, an INA219 current measurement in
**External** mode. The tester never drives an energetic load.

## Build

```
pio run              # build
pio run -t upload    # flash a connected CoreS3
pio device monitor   # 115200 baud console
```

Current footprint: RAM 22.4%, flash 11.8% of the 6.5 MB app partition.

## Feature flags

Everything the spec describes is implemented, but the subsystems that are not
yet wired on the bench are compiled out by default. With a flag at `0` the UI
still behaves exactly as it will with the hardware present — the control is
live, the state machine runs — and the hardware call is replaced by a line on
the USB console. All flags live at the top of [config.h](src/config.h).

| Flag | Default | Effect when `0` |
|---|---|---|
| `FEATURE_MSP_UART` | `0` | MSP frames are hex-dumped to the console instead of Grove Port C |
| `FEATURE_BMP280_SIM` | `0` | Pressure selection is tracked and logged; no I2C slave is started |
| `FEATURE_EXT_LOADS` | `0` | Load selection is tracked and logged; no load pins are driven |
| `FEATURE_INA219` | `1` | Sensor reads are skipped; External shows `NO SENSOR` |

Both the all-off and all-on configurations build warning-free.

## Wiring

| Signal | Pin | Where | Direction |
|---|---|---|---|
| WH Board power-good status | GPIO 5 | M-Bus | in, pull-up |
| WH Board arm status | GPIO 6 | M-Bus | in, pull-up |
| WH Board fire status | GPIO 7 | M-Bus | in, pull-up |
| Green status output | GPIO 8 | Grove Port B | out |
| Red error output | GPIO 9 | Grove Port B | out |
| INA219 SDA / SCL | GPIO 12 / 11 | Internal I2C (M-Bus) | I2C master, **0x41** |
| MSP TX / RX | GPIO 17 / 18 | Grove Port C | UART1, 115200 8N1 |
| BMP280 sim SDA / SCL | **unassigned** | — | I2C slave, 0x76 |
| Load select: short / 0.22Ω / 1Ω | GPIO 10 / 14 / 16 | M-Bus | out, active high |

GPIO 5/6/7 and the load-select lines come out on the M-Bus, so a base module is
required. Grove Port B carries the indicator outputs and Port C the MSP UART;
Grove Port A (GPIO 1/2) is currently unused.

### INA219 on the internal I2C bus

The INA219 shares the CoreS3's internal bus (GPIO 12 SDA / 11 SCL) with the PMU,
touch controller, IMU, RTC and audio codecs. M5Unified owns that bus, so the
firmware talks to the sensor through `M5.In_I2C` — which serialises access with
M5Unified's own traffic — instead of starting an Arduino `Wire` instance on the
same pins. The Adafruit_INA219 library only accepts a `TwoWire`, so
[current_sensor.cpp](src/hal/current_sensor.cpp) drives the configuration,
calibration and current registers directly, with the same 32V/2A values.

**The sensor must not be at the default address 0x40** — that is the CoreS3's
ES7210 microphone codec. Bridge the A0 jumper on the Adafruit breakout for
**0x41** (`kIna219Address`). `config.h` refuses to build with an address that
collides with a known internal device.

On boot the firmware prints an internal bus scan (the CoreS3's own devices
always appear), and the sensor is re-probed every 2 s, so one connected after
boot — or one that stops responding — is picked up without a reset.

`M5.config().output_power` (the 5V rail to the Grove ports) is left `true`. It
is not a path to the WH Board; the spec's "never power an energetic load" rule
is upheld by never driving one.

### Pin budget — needs your decision

The CoreS3 has exactly three uncommitted GPIO left (10, 14, 16) once the status
inputs, indicator outputs, INA219 and MSP UART are placed. The load selector
claims all three, which leaves **no pins for the BMP280 simulator**. Its pins
are therefore `-1`, and enabling `FEATURE_BMP280_SIM` is a deliberate compile
error until a base module or a reallocation is decided. This is the open item
the spec raised in §3.1.

`config.h` now carries compile-time guards that reject any duplicate assignment
or any pin the CoreS3 reserves for itself (LCD CS, SD CS, internal I2C, I2S
speaker, M-Bus SPI, USB UART). This was added after GPIO 13 — the speaker's I2S
data line — was found assigned to a load-select output.

Input polarity (`kInputsActiveHigh`) and the internal pull-up
(`kInputsUsePullup`) are configurable, because the WH Board's drive polarity is not
in the spec. Inputs are debounced 15 ms.

## Screens

All three share one visual language defined in [theme.h](src/ui/theme.h) —
palette, panel style, header bar, indicator lamp, caption. Screens compose those
pieces and never define their own colours or paddings.

**Home** — mode select. Footer shows which optional subsystems this build has
compiled in, so the bench operator can see at a glance what is live.

**Internal** — left panel: Ground/Air pressure switch, Arm switch, FIRE button.
Right panel: five lamps (Power, Arm, Fire, Error, Status). The Power/Arm/Fire
lamps mirror the WH Board status inputs; the Error and Status lamps mirror the
tester's own output pins, blink phase included, so what is on screen is what is
on the connector. Header chip shows the state-machine state.

**External** — four-way load selector (Open / Short / 0.22Ω / 1Ω), the measured
current, and a fire-activated lamp. Header chip shows ACTIVE/INACTIVE. Current
is the only quantity this screen reports: no voltage, power or zero-offset
figure is shown, and the sensor HAL reads only the current register.

## Internal state machine

[internal_sm.cpp](src/logic/internal_sm.cpp) drives the two indicator outputs
from the three status inputs, using the cadences in spec §6:

| State | Entry condition | Green (GPIO 8) | Red (GPIO 9) |
|---|---|---|---|
| Idle | no power | off | off |
| Powered | power good, not armed | 166 ms | off |
| Arming | arm asserted, first 10 s | 1000 ms | off |
| Armed | arm asserted, after 10 s | 600 ms | off |
| Fired | fire asserted while armed | steady on | off |
| Error | invalid combination | 100 ms | 100 ms |

Error is raised on fire-without-arm, fire-without-power, or arm-without-power,
and clears when the offending inputs go away. Cadences are the *toggle*
interval, so a full cycle is twice the figure.

**Entry gate.** Entering the Internal screen with arm or fire already asserted
goes straight to Error: the tester has commanded neither, so the WH Board is not in a
known-safe starting state. The fault is latched and clears only once the WH Board
drops both lines — waiting does not clear it. A live invalid combination
outranks the latched entry fault, so the more specific cause is what shows.

**Fired.** A fire input received *following arming* is a valid fire and enters
Fired immediately — during the 10 s settle window as well as after it, with no
wait. Passing through Fired does not restart the arming clock. Steady green is
not a cadence the spec defines; it was chosen because it cannot be confused with
any of the blink rates.

## MSP link

[msp.cpp](src/msp/msp.cpp) implements MSP v1 (`$M<`, XOR checksum) with a full
receive parser for `$M>` replies, plus MSP v2 (`$X<`, CRC8/DVB-S2) encoding for
function IDs above 255. `MSP_STATUS` (101) is polled at 10 Hz and decoded for
the arm flag; `MSP_SET_RAW_RC` (200) is sent at 20 Hz as the RC heartbeat.

ARM and FIRE are carried as RC aux channels (AUX1 level, AUX2 200 ms pulse),
which is how MSP arms a MultiWii/Betaflight-style target — **this mapping needs
your confirmation**, see open items below. It is confined to three constants and
two functions in [msp.h](src/msp/msp.h); nothing else in the firmware knows how
a command is framed.

## Fire alert

[fire_alert.cpp](src/ui/fire_alert.cpp) is the single shared handler called from
both test screens. It fades a full-screen red overlay on `lv_layer_top()` over
200 ms and plays the explosion sample, with a 1.5 s lockout against re-triggers.

The sample is `mixkit-fuel-explosion-1705.wav` (44.1 kHz 24-bit stereo)
converted to 16 kHz mono 16-bit PCM, trimmed to 1.2 s from the onset, peak
normalised and fade-ended, baked into flash as a 37.5 KB array and played with
`Speaker.playRaw()` — no SD card and no decoder library. Regenerate with:

```
python3 tools/wav_to_pcm.py mixkit-fuel-explosion-1705.wav \
    src/assets/boom_pcm.h src/assets/boom_pcm.cpp
```

## Display byte order

LVGL and M5GFX must agree on the in-memory layout of a pixel, and they express
it with **opposite polarity**, which makes this easy to get wrong:

- `LV_COLOR_16_SWAP == 1` makes LVGL render in the panel's wire order, a layout
  bit-identical to LovyanGFX's `swap565_t`.
- `writePixels(..., swap)` means *"the source is in native order, swap it for
  me"*, which selects `rgb565_t` instead.

So the argument must be the **inverse** of `LV_COLOR_16_SWAP`.
[display.cpp](src/hal/display.cpp) derives it (`kSwapForPanel`) rather than
hard-coding it, so the pair cannot drift apart. Get it wrong and every pixel's
bitfields are reinterpreted: red renders as blue, while white survives intact
because `0xFFFF` is symmetric — which makes the fault easy to misread as a
backlight or contrast problem.

## Decisions I made that you should check

The spec left these open and they were not among the four questions I asked, so
I implemented the conservative reading:

1. **Fire is UI-interlocked behind Arm** (spec §8) — the FIRE button is greyed
   out unless Arm is on, *and* `msp::sendFire()` refuses when not armed.
1. **The fire alert now fires only for a valid, post-arm fire.** It used to
   trigger on any rising edge of the fire input. A fire without arm is a fault,
   so it raises the Error state and lights the Fire lamp but plays no alert.
   Say so if you want the alert on a spurious fire too.
2. **Navigating Home disarms** rather than prompting. Leaving Internal sends
   `arm 0` and clears the switch; leaving External returns the selector to Open.
   No confirmation dialog. Say the word if you want a prompt instead.
3. **No armed-state timeout.** The 10 s figure in §6 is the arming→armed settle
   window, not an expiry.
4. **INA219 calibration matches Adafruit's `setCalibration_32V_2A()`** — the 1200 mA threshold
   in §1 rules out the finer 1 A and 400 mA ranges.
5. **±25 mA hysteresis** on the 1200 mA threshold, so a reading sitting on the
   limit cannot chatter the indicator.
6. **Calibration zeroes the sensor.** Spec §7 flagged this as ambiguous.
   Selecting **Open Circuit** captures the INA219 zero offset — the only state
   with genuinely no current flowing — deferred 400 ms so the sample is taken
   after the load actually changed. The offset is subtracted from every later
   reading but is not displayed. Tell me if you meant a per-unit
   expected-current baseline instead.

## Open items still outstanding

- **MSP command mapping.** RC-aux arming is the default; if the WH Board exposes
  dedicated MSP function IDs for arm/fire, give me the IDs and payloads.
- **BMP280 calibration block.** The register map, pointer auto-increment and
  I2C-slave plumbing are complete, but the advertised `dig_T*`/`dig_P*`
  coefficients are placeholders. A real driver runs the Bosch compensation
  polynomial, so the raw registers we publish must be the pre-compensation
  values that yield the target pressure for whatever coefficients we advertise.
  Deriving that pair is required before the simulator reads correctly at the
  WH Board. Flagged in [bmp280_sim.cpp](src/sim/bmp280_sim.cpp).
- **Load-select pins** (GPIO 13/14/15) are placeholders until the load board
  exists.
- **WH Board input polarity** — confirm active-high and whether the WH Board drives both
  rails, so the pull-ups can be dropped.
- **External fire detection** is threshold-based per §1. If the WH Board reports fire
  over MSP instead, that path is a one-line change in `externalRefresh()`.
