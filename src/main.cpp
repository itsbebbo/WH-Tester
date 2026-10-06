// Arm/Fire Circuit Functional Tester -- M5Stack CoreS3
//
// Bench equipment that commands a WH Board over MSP and
// independently observes its outputs: three digital status lines in internal
// mode, an INA219 current measurement in external mode. The tester never
// drives an energetic load.
//
// See README.md for wiring and for the specification items still open.

#include <Arduino.h>
#include <M5Unified.h>

#include "config.h"
#include "hal/audio.h"
#include "hal/current_sensor.h"
#include "hal/display.h"
#include "hal/io.h"
#include "logic/internal_sm.h"
#include "msp/msp.h"
#include "sim/bmp280_sim.h"
#include "ui/ui.h"

void setup() {
    hal::displayBegin();  // brings up M5Unified as well as LVGL

    Serial.begin(115200);
    Serial.println();
    Serial.println("=== Arm/Fire Functional Tester ===");

    hal::ioBegin();
    hal::audioBegin();
    hal::currentSensorBegin();
    sim::bmp280Begin();
    msp::begin();
    logic::internalBegin();

    ui::uiBegin();
}

void loop() {
    hal::ioService();
    hal::currentSensorService();
    msp::service();
    logic::internalService();

    ui::uiService();
    hal::displayService();

    delay(kUiTickMs);
}
