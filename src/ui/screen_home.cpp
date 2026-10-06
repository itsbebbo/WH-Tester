#include <lvgl.h>
#include <stdio.h>

#include "config.h"
#include "ui/screens.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace ui {
namespace {

lv_obj_t* g_screen = nullptr;

void internalCb(lv_event_t* e) { (void)e; showInternal(); }
void externalCb(lv_event_t* e) { (void)e; showExternal(); }

// Both mode buttons share one builder so they cannot drift apart visually.
lv_obj_t* makeModeButton(lv_obj_t* parent, const char* title,
                         lv_coord_t x, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 146, 116);
    lv_obj_set_pos(btn, x, kBodyY + 30);
    lv_obj_set_style_bg_color(btn, kColPanel, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, kColPanelEdge, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, kColAccent, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* head = lv_label_create(btn);
    lv_label_set_text(head, title);
    lv_obj_set_style_text_color(head, kColText, 0);
    lv_obj_set_style_text_font(head, &lv_font_montserrat_20, 0);
    lv_obj_center(head);

    return btn;
}

}  // namespace

lv_obj_t* homeScreen() {
    if (g_screen) return g_screen;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_screen, kColBg, 0);
    lv_obj_clear_flag(g_screen, LV_OBJ_FLAG_SCROLLABLE);

    makeHeader(g_screen, "Arm / Fire Tester", false, nullptr);

    lv_obj_t* prompt = makeCaption(g_screen, "Select Test Mode");
    lv_obj_set_style_text_font(prompt, &lv_font_montserrat_14, 0);
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, kBodyY + 8);

    makeModeButton(g_screen, "Internal", kGap, internalCb);
    makeModeButton(g_screen, "External", kScreenW - 146 - kGap, externalCb);

    // Footer states which optional subsystems this build has compiled in, so
    // the bench operator can see at a glance what is live.
    char footer[96];
    snprintf(footer, sizeof(footer), "MSP %s   BMP280 %s   Loads %s   INA219 %s",
             FEATURE_MSP_UART ? "on" : "off",
             FEATURE_BMP280_SIM ? "on" : "off",
             FEATURE_EXT_LOADS ? "on" : "off",
             FEATURE_INA219 ? "on" : "off");
    lv_obj_t* flags = makeCaption(g_screen, footer);
    lv_obj_align(flags, LV_ALIGN_BOTTOM_MID, 0, -6);

    return g_screen;
}

}  // namespace ui
