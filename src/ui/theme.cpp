#include "ui/theme.h"

namespace ui {

lv_color_t kColBg;
lv_color_t kColPanel;
lv_color_t kColPanelEdge;
lv_color_t kColText;
lv_color_t kColTextDim;
lv_color_t kColAccent;
lv_color_t kColGreen;
lv_color_t kColRed;
lv_color_t kColAmber;
lv_color_t kColOff;

namespace {

lv_style_t g_style_panel;
lv_style_t g_style_header;
lv_style_t g_style_chip;
lv_style_t g_style_caption;
bool       g_ready = false;

}  // namespace

void themeBegin() {
    if (g_ready) return;

    kColBg        = lv_color_hex(0x101418);
    kColPanel     = lv_color_hex(0x1B2128);
    kColPanelEdge = lv_color_hex(0x2E3742);
    kColText      = lv_color_hex(0xE8EDF2);
    kColTextDim   = lv_color_hex(0x8A96A3);
    kColAccent    = lv_color_hex(0x2F81F7);
    kColGreen     = lv_color_hex(0x2ECC71);
    kColRed       = lv_color_hex(0xE74C3C);
    kColAmber     = lv_color_hex(0xF2A83B);
    kColOff       = lv_color_hex(0x323A44);

    lv_style_init(&g_style_panel);
    lv_style_set_bg_color(&g_style_panel, kColPanel);
    lv_style_set_bg_opa(&g_style_panel, LV_OPA_COVER);
    lv_style_set_border_color(&g_style_panel, kColPanelEdge);
    lv_style_set_border_width(&g_style_panel, 1);
    lv_style_set_radius(&g_style_panel, 6);
    lv_style_set_pad_all(&g_style_panel, kGap);
    lv_style_set_text_color(&g_style_panel, kColText);

    lv_style_init(&g_style_header);
    lv_style_set_bg_color(&g_style_header, lv_color_hex(0x161C22));
    lv_style_set_bg_opa(&g_style_header, LV_OPA_COVER);
    lv_style_set_border_color(&g_style_header, kColPanelEdge);
    lv_style_set_border_width(&g_style_header, 1);
    lv_style_set_border_side(&g_style_header, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_radius(&g_style_header, 0);
    lv_style_set_pad_all(&g_style_header, 0);

    lv_style_init(&g_style_chip);
    lv_style_set_bg_opa(&g_style_chip, LV_OPA_COVER);
    lv_style_set_radius(&g_style_chip, 8);
    lv_style_set_pad_hor(&g_style_chip, 6);
    lv_style_set_pad_ver(&g_style_chip, 2);
    lv_style_set_text_color(&g_style_chip, lv_color_hex(0x0E1216));
    lv_style_set_text_font(&g_style_chip, &lv_font_montserrat_12);

    lv_style_init(&g_style_caption);
    lv_style_set_text_color(&g_style_caption, kColTextDim);
    lv_style_set_text_font(&g_style_caption, &lv_font_montserrat_12);

    g_ready = true;
}

lv_obj_t* makeHeader(lv_obj_t* parent, const char* title, bool with_back,
                     lv_event_cb_t back_cb) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, &g_style_header, 0);
    lv_obj_set_size(bar, kScreenW, kHeaderH);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    if (with_back) {
        lv_obj_t* back = lv_btn_create(bar);
        lv_obj_remove_style_all(back);
        lv_obj_set_size(back, kBackBtnW, kHeaderH);
        lv_obj_set_pos(back, 0, 0);
        lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
        if (back_cb) lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* glyph = lv_label_create(back);
        lv_label_set_text(glyph, LV_SYMBOL_LEFT " Home");
        lv_obj_set_style_text_color(glyph, kColAccent, 0);
        lv_obj_set_style_text_font(glyph, &lv_font_montserrat_14, 0);
        // The bar's bottom border shrinks its content box from every side, so
        // a centred child sits a pixel low; kBackBtnNudge lifts the glyph clear
        // of that and off the bottom rule.
        lv_obj_align(glyph, LV_ALIGN_CENTER, 0, kBackBtnNudge);
    }

    lv_obj_t* label = lv_label_create(bar);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, kColText, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* chip = lv_label_create(bar);
    lv_obj_add_style(chip, &g_style_chip, 0);
    lv_label_set_text(chip, "");
    lv_obj_set_style_bg_color(chip, kColOff, 0);
    lv_obj_align(chip, LV_ALIGN_RIGHT_MID, -kGap, 0);
    return chip;
}

void setChip(lv_obj_t* chip, const char* text, lv_color_t colour) {
    lv_label_set_text(chip, text);
    lv_obj_set_style_bg_color(chip, colour, 0);
    lv_obj_align(chip, LV_ALIGN_RIGHT_MID, -kGap, 0);
}

lv_obj_t* makePanel(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &g_style_panel, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

Lamp makeLamp(lv_obj_t* parent, const char* text) {
    Lamp lamp{};

    lamp.row = lv_obj_create(parent);
    lv_obj_remove_style_all(lamp.row);
    lv_obj_set_size(lamp.row, LV_PCT(100), kLampRowH);
    lv_obj_clear_flag(lamp.row, LV_OBJ_FLAG_SCROLLABLE);

    lamp.led = lv_led_create(lamp.row);
    lv_obj_set_size(lamp.led, kLampSize, kLampSize);
    // Inset from the row's left edge so the glow is not sliced by the panel
    // border -- see kLampInset.
    lv_obj_align(lamp.led, LV_ALIGN_LEFT_MID, kLampInset, 0);

    // The default theme gives an LED shadow_width of dpx(15) and spread of
    // dpx(5) -- about 10 px of glow at this DPI, which is oversized for a
    // 16 px indicator and overruns the panel padding. Pinned to explicit
    // pixels so it stays inside kLampInset regardless of the display's DPI.
    lv_obj_set_style_shadow_width(lamp.led, kLampGlowWidth, 0);
    lv_obj_set_style_shadow_spread(lamp.led, kLampGlowSpread, 0);

    lv_led_set_color(lamp.led, kColOff);
    lv_led_set_brightness(lamp.led, 40);

    lamp.label = lv_label_create(lamp.row);
    lv_label_set_text(lamp.label, text);
    lv_obj_set_style_text_color(lamp.label, kColText, 0);
    lv_obj_set_style_text_font(lamp.label, &lv_font_montserrat_14, 0);
    lv_obj_align(lamp.label, LV_ALIGN_LEFT_MID, kLampInset + kLampSize + 8, 0);

    return lamp;
}

void setLamp(const Lamp& lamp, bool lit, lv_color_t colour) {
    lv_led_set_color(lamp.led, lit ? colour : kColOff);
    lv_led_set_brightness(lamp.led, lit ? 255 : 40);
    lv_obj_set_style_text_color(lamp.label, lit ? kColText : kColTextDim, 0);
}

lv_obj_t* makeCaption(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &g_style_caption, 0);
    return label;
}

}  // namespace ui
