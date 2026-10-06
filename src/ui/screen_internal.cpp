#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "hal/io.h"
#include "logic/internal_sm.h"
#include "msp/msp.h"
#include "sim/bmp280_sim.h"
#include "ui/screens.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace ui {
namespace {

lv_obj_t* g_screen   = nullptr;
lv_obj_t* g_chip     = nullptr;
lv_obj_t* g_sw_press = nullptr;
lv_obj_t* g_sw_arm   = nullptr;
lv_obj_t* g_btn_fire = nullptr;
lv_obj_t* g_lbl_pressure = nullptr;
lv_obj_t* g_lbl_arm_hint = nullptr;
lv_obj_t* g_lbl_state    = nullptr;

Lamp g_lamp_power;
Lamp g_lamp_arm;
Lamp g_lamp_fire;
Lamp g_lamp_error;
Lamp g_lamp_status;

void backCb(lv_event_t* e) { (void)e; showHome(); }

void pressureCb(lv_event_t* e) {
    const bool air = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sim::setPressure(air ? sim::Altitude::kAir : sim::Altitude::kGround);
}

void armCb(lv_event_t* e) {
    const bool armed = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    msp::sendArm(armed);
}

void fireCb(lv_event_t* e) {
    (void)e;
    msp::sendFire();
}

// Fire is interlocked behind Arm in the UI as well as in the MSP layer, so a
// fire command can never be composed while disarmed.
void applyFireInterlock() {
    if (msp::armRequested()) {
        lv_obj_clear_state(g_btn_fire, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(g_btn_fire, LV_STATE_DISABLED);
    }
}

lv_obj_t* makeSwitch(lv_obj_t* parent, lv_coord_t y, lv_event_cb_t cb) {
    lv_obj_t* sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 56, 28);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, 0, y);
    lv_obj_set_style_bg_color(sw, kColOff, 0);
    lv_obj_set_style_bg_color(sw, kColAccent, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return sw;
}

}  // namespace

lv_obj_t* internalScreen() {
    if (g_screen) return g_screen;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_screen, kColBg, 0);
    lv_obj_clear_flag(g_screen, LV_OBJ_FLAG_SCROLLABLE);

    g_chip = makeHeader(g_screen, "Internal", true, backCb);

    // ---- left panel: controls --------------------------------------------
    lv_obj_t* controls = makePanel(g_screen);
    lv_obj_set_size(controls, 168, kBodyH - 2 * kGap);
    lv_obj_set_pos(controls, kGap, kBodyY + kGap);

    lv_obj_t* cap_press = makeCaption(controls, "PRESSURE");
    lv_obj_align(cap_press, LV_ALIGN_TOP_LEFT, 0, 4);
    g_lbl_pressure = lv_label_create(controls);
    lv_obj_set_style_text_color(g_lbl_pressure, kColText, 0);
    lv_obj_set_style_text_font(g_lbl_pressure, &lv_font_montserrat_14, 0);
    lv_obj_align(g_lbl_pressure, LV_ALIGN_TOP_LEFT, 0, 20);
    g_sw_press = makeSwitch(controls, 6, pressureCb);

    lv_obj_t* cap_arm = makeCaption(controls, "ARM");
    lv_obj_align(cap_arm, LV_ALIGN_TOP_LEFT, 0, 54);
    g_lbl_arm_hint = lv_label_create(controls);
    lv_label_set_text(g_lbl_arm_hint, "(Not Armed)");
    lv_obj_set_style_text_color(g_lbl_arm_hint, kColText, 0);
    lv_obj_set_style_text_font(g_lbl_arm_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(g_lbl_arm_hint, LV_ALIGN_TOP_LEFT, 0, 70);
    g_sw_arm = makeSwitch(controls, 56, armCb);

    g_btn_fire = lv_btn_create(controls);
    lv_obj_remove_style_all(g_btn_fire);
    lv_obj_set_size(g_btn_fire, 156, 52);
    lv_obj_align(g_btn_fire, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_radius(g_btn_fire, 6, 0);
    lv_obj_set_style_bg_opa(g_btn_fire, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_btn_fire, kColRed, 0);
    lv_obj_set_style_bg_color(g_btn_fire, lv_color_hex(0xFF7A6A), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(g_btn_fire, kColOff, LV_STATE_DISABLED);
    lv_obj_add_event_cb(g_btn_fire, fireCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* fire_label = lv_label_create(g_btn_fire);
    lv_label_set_text(fire_label, "FIRE");
    lv_obj_set_style_text_font(fire_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(fire_label, kColText, 0);
    lv_obj_set_style_text_color(fire_label, kColTextDim, LV_STATE_DISABLED);
    lv_obj_center(fire_label);

    g_lbl_state = lv_label_create(controls);
    lv_obj_set_style_text_font(g_lbl_state, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_lbl_state, kColTextDim, 0);
    lv_obj_align(g_lbl_state, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(g_lbl_state, "");

    // ---- right panel: indicators -----------------------------------------
    lv_obj_t* lamps = makePanel(g_screen);
    lv_obj_set_size(lamps, kScreenW - 168 - 3 * kGap, kBodyH - 2 * kGap);
    lv_obj_set_pos(lamps, 168 + 2 * kGap, kBodyY + kGap);
    lv_obj_set_flex_flow(lamps, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(lamps, LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    g_lamp_power  = makeLamp(lamps, "Power");
    g_lamp_arm    = makeLamp(lamps, "Arm");
    g_lamp_fire   = makeLamp(lamps, "Fire");
    g_lamp_error  = makeLamp(lamps, "Error");
    g_lamp_status = makeLamp(lamps, "Status");

    applyFireInterlock();
    return g_screen;
}

void internalRefresh() {
    if (!g_screen) return;

    // Input lamps mirror the WH Board status lines directly.
    setLamp(g_lamp_power, hal::powerGood(), kColGreen);
    setLamp(g_lamp_arm, hal::armStatus(), kColAmber);
    setLamp(g_lamp_fire, hal::fireStatus(), kColRed);

    // Output lamps mirror the physical pins, blink phase included, so what is
    // on screen is what is on the connector.
    setLamp(g_lamp_error, hal::errorRedLit(), kColRed);
    setLamp(g_lamp_status, hal::statusGreenLit(), kColGreen);

    lv_label_set_text(g_lbl_pressure,
                      sim::pressure() == sim::Altitude::kAir ? "Air"
                                                             : "Ground");

    // Reflects the toggle's own position, not the WH Board's arm status
    // input -- that is what the Arm lamp on the right shows.
    const bool arm_on = msp::armRequested();
    lv_label_set_text(g_lbl_arm_hint, arm_on ? "(Armed)" : "(Not Armed)");
    lv_obj_set_style_text_color(g_lbl_arm_hint, arm_on ? kColAmber : kColTextDim, 0);

    const logic::InternalState state = logic::internalState();
    lv_color_t chip_col = kColOff;
    switch (state) {
        case logic::InternalState::kIdle:   chip_col = kColOff;   break;
        case logic::InternalState::kPower:  chip_col = kColGreen; break;
        case logic::InternalState::kArming: chip_col = kColAmber; break;
        case logic::InternalState::kArmed:  chip_col = kColGreen; break;
        case logic::InternalState::kFired:  chip_col = kColRed;   break;
        case logic::InternalState::kError:  chip_col = kColRed;   break;
    }
    setChip(g_chip, logic::internalStateName(), chip_col);

    char detail[48];
    if (state == logic::InternalState::kError) {
        snprintf(detail, sizeof(detail), "%s", logic::internalErrorText());
    } else if (state == logic::InternalState::kArming) {
        snprintf(detail, sizeof(detail), "settling %lus",
                 static_cast<unsigned long>((logic::armingRemainingMs() + 999) / 1000));
    } else if (state == logic::InternalState::kFired) {
        snprintf(detail, sizeof(detail), "fire input active");
    } else {
        snprintf(detail, sizeof(detail), "%s",
                 msp::status().link_up ? "WH Board link up"
                                       : "WH Board link down");
    }
    lv_label_set_text(g_lbl_state, detail);

    applyFireInterlock();
}

void internalOnEnter() {
    logic::internalReset();
    logic::takeFireEvent();  // discard any edge latched while off-screen
}

void internalOnLeave() {
    // Leaving the screen disarms, so the WH Board is never left armed by a
    // navigation action.
    if (msp::armRequested()) {
        msp::sendArm(false);
        lv_obj_clear_state(g_sw_arm, LV_STATE_CHECKED);
        applyFireInterlock();
    }
}

}  // namespace ui
