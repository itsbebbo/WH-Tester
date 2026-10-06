// LVGL 8.3 configuration for the CoreS3 tester.
// Reached via -D LV_CONF_INCLUDE_SIMPLE with include/ on the include path.
#pragma once
#define LV_CONF_H

#define LV_COLOR_DEPTH        16
#define LV_COLOR_16_SWAP      1     // renders in the panel's wire order; the flush
                                    // callback derives its swap flag from this

#define LV_MEM_CUSTOM         0
#define LV_MEM_SIZE           (48U * 1024U)

#define LV_DISP_DEF_REFR_PERIOD  16
#define LV_INDEV_DEF_READ_PERIOD 16

#define LV_TICK_CUSTOM        0

#define LV_USE_PERF_MONITOR   0
#define LV_USE_MEM_MONITOR    0
#define LV_USE_LOG            0

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_34 1
#define LV_FONT_DEFAULT       &lv_font_montserrat_14

#define LV_USE_THEME_DEFAULT  1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0

// Widgets used by this application. Everything else stays off to keep the
// binary small.
#define LV_USE_ARC        0
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0
#define LV_USE_MSGBOX     1

#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        1
#define LV_USE_LIST       0
#define LV_USE_METER      0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
