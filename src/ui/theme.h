// Shared visual language for every screen: palette, geometry, and the small
// set of composite widgets the three screens are built from. Screens must not
// invent their own colours, paddings or widget shapes -- add them here instead.
#pragma once

#include <lvgl.h>

namespace ui {

// ---- palette -------------------------------------------------------------
extern lv_color_t kColBg;         // screen background
extern lv_color_t kColPanel;      // card / panel fill
extern lv_color_t kColPanelEdge;  // card border
extern lv_color_t kColText;       // primary text
extern lv_color_t kColTextDim;    // secondary text
extern lv_color_t kColAccent;     // interactive / selected
extern lv_color_t kColGreen;      // good / active
extern lv_color_t kColRed;        // error / fire
extern lv_color_t kColAmber;      // transitional
extern lv_color_t kColOff;        // unlit indicator

// ---- geometry ------------------------------------------------------------
constexpr lv_coord_t kScreenW  = 320;
constexpr lv_coord_t kScreenH  = 240;
constexpr lv_coord_t kHeaderH  = 34;
constexpr lv_coord_t kBodyY    = kHeaderH;
constexpr lv_coord_t kBodyH    = kScreenH - kHeaderH;
constexpr lv_coord_t kGap      = 6;

// Back button in the header. The width is the touch target, not the text
// extent -- it stays clear of the centred title, which starts at x=160.
constexpr lv_coord_t kBackBtnW     = 64;
constexpr lv_coord_t kBackBtnNudge = -2;  // lift the glyph off the bottom rule

// Call once after lv_init().
void themeBegin();

// ---- composite widgets ---------------------------------------------------

// Header bar common to all three screens: optional back chevron on the left,
// centred title, and a right-hand status chip whose text and colour the screen
// updates. Returns the chip so the caller can keep a handle on it.
lv_obj_t* makeHeader(lv_obj_t* parent, const char* title, bool with_back,
                     lv_event_cb_t back_cb);

void setChip(lv_obj_t* chip, const char* text, lv_color_t colour);

// A panel: the only container style screens should use.
lv_obj_t* makePanel(lv_obj_t* parent);

// Indicator-lamp geometry. The glow extends kLampGlowSpread + kLampGlowWidth/2
// beyond the lamp on every side, so kLampInset must exceed that minus the
// panel's own padding, or the leading edge of the glow is clipped by the
// border. kLampRowH must also leave that much clearance above and below.
constexpr lv_coord_t kLampSize       = 16;
constexpr lv_coord_t kLampInset      = 6;   // from the row's left edge
constexpr lv_coord_t kLampGlowWidth  = 10;
constexpr lv_coord_t kLampGlowSpread = 2;
constexpr lv_coord_t kLampRowH       = 26;

// Minimum panel height that can hold one lamp row without clipping it:
// row height plus the panel's border and padding on both sides.
constexpr lv_coord_t kLampPanelMinH = kLampRowH + 2 * (kGap + 1);

// A labelled indicator lamp. `led` is the lamp; the label sits to its right.
struct Lamp {
    lv_obj_t* row;
    lv_obj_t* led;
    lv_obj_t* label;
};

Lamp makeLamp(lv_obj_t* parent, const char* text);

// Sets a lamp's colour and lit state in one call, so every screen renders the
// same states identically.
void setLamp(const Lamp& lamp, bool lit, lv_color_t colour);

// A caption above a control, used to keep control labelling uniform.
lv_obj_t* makeCaption(lv_obj_t* parent, const char* text);

}  // namespace ui
