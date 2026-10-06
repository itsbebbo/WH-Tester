#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "hal/current_sensor.h"
#include "hal/io.h"
#include "ui/screens.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace ui {
namespace {

lv_obj_t* g_screen    = nullptr;
lv_obj_t* g_chip      = nullptr;
lv_obj_t* g_modes     = nullptr;
lv_obj_t* g_lbl_current = nullptr;
lv_obj_t* g_lbl_hint    = nullptr;
Lamp      g_lamp_fire;

bool     g_prev_active = false;
uint32_t g_zero_due    = 0;  // millis() at which to capture the zero offset

// Time for the load relay/FET to settle and the INA219 to produce a reading
// taken with the new load actually connected.
constexpr uint32_t kLoadSettleMs = 400;

// Button-matrix entries, and the load each one applies. The 0.22 ohm
// calibration load is not offered on this screen.
const char* kModeMap[] = {"Open", "Short", "WH", ""};

const hal::Load kModeLoad[] = {
    hal::Load::kOpen,
    hal::Load::kShort,
    hal::Load::kDevice,
};

constexpr uint32_t kModeCount = sizeof(kModeLoad) / sizeof(kModeLoad[0]);

// Explanation of what each load is for, shown under the selector so the
// operator does not have to consult the spec.
const char* kModeHint[] = {
    "Open Circuit - No Warhead",
    "Short Circuit - Fault Injection",
    "WH Present - 1 ohm Simulated WH",
};

void backCb(lv_event_t* e) { (void)e; showHome(); }

void modeCb(lv_event_t* e) {
    lv_obj_t* mx = lv_event_get_target(e);
    const uint32_t index = lv_btnmatrix_get_selected_btn(mx);
    if (index >= kModeCount) return;

    const hal::Load load = kModeLoad[index];
    hal::setLoad(load);

    // Zeroing has to happen with no current flowing, which is Open Circuit.
    // Deferred, because the INA219 sample in hand was taken before the load
    // changed.
    g_zero_due = (load == hal::Load::kOpen) ? millis() + kLoadSettleMs : 0;

    lv_label_set_text(g_lbl_hint, kModeHint[index]);
}

}  // namespace

lv_obj_t* externalScreen() {
    if (g_screen) return g_screen;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_screen, kColBg, 0);
    lv_obj_clear_flag(g_screen, LV_OBJ_FLAG_SCROLLABLE);

    g_chip = makeHeader(g_screen, "External", true, backCb);

    // ---- load selector ---------------------------------------------------
    g_modes = lv_btnmatrix_create(g_screen);
    lv_btnmatrix_set_map(g_modes, kModeMap);
    lv_btnmatrix_set_one_checked(g_modes, true);
    lv_obj_set_size(g_modes, kScreenW - 2 * kGap, 38);
    lv_obj_set_pos(g_modes, kGap, kBodyY + kGap);
    lv_obj_set_style_bg_color(g_modes, kColBg, 0);
    lv_obj_set_style_border_width(g_modes, 0, 0);
    lv_obj_set_style_pad_all(g_modes, 0, 0);
    lv_obj_set_style_pad_gap(g_modes, 4, 0);
    lv_obj_set_style_bg_color(g_modes, kColPanel, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(g_modes, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(g_modes, kColPanelEdge, LV_PART_ITEMS);
    lv_obj_set_style_border_width(g_modes, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(g_modes, 6, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_modes, kColTextDim, LV_PART_ITEMS);
    lv_obj_set_style_text_font(g_modes, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_modes, kColAccent, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(g_modes, kColText, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_event_cb(g_modes, modeCb, LV_EVENT_VALUE_CHANGED, nullptr);

    for (uint32_t i = 0; i < kModeCount; ++i) {
        lv_btnmatrix_set_btn_ctrl(g_modes, i, LV_BTNMATRIX_CTRL_CHECKABLE);
    }
    lv_btnmatrix_set_btn_ctrl(g_modes, 0, LV_BTNMATRIX_CTRL_CHECKED);

    g_lbl_hint = makeCaption(g_screen, kModeHint[0]);
    lv_obj_align(g_lbl_hint, LV_ALIGN_TOP_MID, 0, kBodyY + 48);

    // ---- current readout -------------------------------------------------
    lv_obj_t* readout = makePanel(g_screen);
    lv_obj_set_size(readout, kScreenW - 2 * kGap, 86);
    lv_obj_set_pos(readout, kGap, kBodyY + 66);

    lv_obj_t* cap = makeCaption(readout, "WH BOARD OUTPUT POWER");
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 2, 0);

    // The measured current is the only value on this screen, so it gets the
    // whole panel.
    g_lbl_current = lv_label_create(readout);
    lv_obj_set_style_text_font(g_lbl_current, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(g_lbl_current, kColText, 0);
    lv_obj_align(g_lbl_current, LV_ALIGN_LEFT_MID, 2, 4);
    lv_label_set_text(g_lbl_current, "---- mA");

    // ---- fire indicator --------------------------------------------------
    // 32 px left only 18 px of content for a 26 px lamp row, clipping the lamp
    // vertically as well as its glow.
    lv_obj_t* strip = makePanel(g_screen);
    lv_obj_set_size(strip, kScreenW - 2 * kGap, kLampPanelMinH);
    lv_obj_set_pos(strip, kGap, kBodyY + 66 + 86 + kGap);

    g_lamp_fire = makeLamp(strip, "WH Fire Activated");

    return g_screen;
}

void externalRefresh() {
    if (!g_screen) return;

    const hal::CurrentReading reading = hal::currentReading();

    if (g_zero_due != 0 && millis() >= g_zero_due &&
        reading.valid && reading.sample_ms >= g_zero_due) {
        hal::captureZeroOffset();
        g_zero_due = 0;
    }

    if (!hal::currentSensorPresent()) {
        lv_label_set_text(g_lbl_current, "-- SENSOR --");
        setChip(g_chip, "NO SENSOR", kColRed);
        setLamp(g_lamp_fire, false, kColRed);
        return;
    }

    char text[32];
    snprintf(text, sizeof(text), "%.1f mA", reading.current_ma);
    lv_label_set_text(g_lbl_current, text);

    // Spec section 1: at or above the threshold the WH Board output is a
    // correct active output; below it, inactive.
    const bool active = hal::outputActive();
    lv_obj_set_style_text_color(g_lbl_current, active ? kColGreen : kColText, 0);
    setChip(g_chip, active ? "ACTIVE" : "INACTIVE", active ? kColGreen : kColOff);
    setLamp(g_lamp_fire, active, kColRed);

    if (active && !g_prev_active) triggerFireAlert("external current threshold");
    g_prev_active = active;
}

void externalOnEnter() {
    // Seed the edge detector, so arriving with the output already above the
    // threshold shows ACTIVE without replaying the alert.
    g_prev_active = hal::outputActive();
}

void externalOnLeave() {
    // Never leave a load connected to the WH Board output.
    hal::setLoad(hal::Load::kOpen);

    // lv_btnmatrix_set_btn_ctrl does not apply the one-checked rule, so the
    // other buttons have to be cleared explicitly.
    for (uint32_t i = 1; i < kModeCount; ++i) {
        lv_btnmatrix_clear_btn_ctrl(g_modes, i, LV_BTNMATRIX_CTRL_CHECKED);
    }
    lv_btnmatrix_set_btn_ctrl(g_modes, 0, LV_BTNMATRIX_CTRL_CHECKED);
    lv_label_set_text(g_lbl_hint, kModeHint[0]);
    g_prev_active = false;
    g_zero_due    = 0;
}

}  // namespace ui
