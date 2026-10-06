#include "hal/display.h"

#include <M5Unified.h>
#include <lvgl.h>

#include "config.h"

namespace hal {
namespace {

constexpr uint32_t kHorRes = 320;
constexpr uint32_t kVerRes = 240;

static_assert(LV_COLOR_DEPTH == 16,
              "flushCb pushes 16-bit pixels; update it if the depth changes");

// LVGL and M5GFX have to agree on the in-memory byte order of a pixel, and the
// two express it with opposite polarity:
//
//   LV_COLOR_16_SWAP == 1  ->  LVGL renders in the panel's wire order, which is
//                              bit-identical to LovyanGFX's swap565_t.
//   writePixels(swap)      ->  "the source is in native order, swap it for me",
//                              which selects rgb565_t instead.
//
// So the argument must be the inverse of LV_COLOR_16_SWAP. Deriving it here
// rather than hard-coding it keeps the pair from drifting apart -- getting it
// wrong reinterprets every pixel's bitfields and the colours come out scrambled
// (white survives, since 0xFFFF is symmetric, which makes it easy to miss).
constexpr bool kSwapForPanel = (LV_COLOR_16_SWAP == 0);

// Two partial buffers, 1/10 screen each, let LVGL render one while the other
// is being pushed by DMA.
constexpr uint32_t kBufLines = kVerRes / 10;

lv_disp_draw_buf_t g_draw_buf;
lv_color_t*        g_buf_a = nullptr;
lv_color_t*        g_buf_b = nullptr;
lv_disp_drv_t      g_disp_drv;
lv_indev_drv_t     g_indev_drv;

uint32_t g_last_tick_ms = 0;

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* pixels) {
    const uint32_t w = area->x2 - area->x1 + 1;
    const uint32_t h = area->y2 - area->y1 + 1;

    M5.Display.startWrite();
    M5.Display.setAddrWindow(area->x1, area->y1, w, h);
    M5.Display.writePixels(reinterpret_cast<uint16_t*>(pixels), w * h, kSwapForPanel);
    M5.Display.endWrite();

    lv_disp_flush_ready(drv);
}

void touchCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    const auto count = M5.Touch.getCount();
    if (count == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    const auto point = M5.Touch.getDetail(0);
    data->state   = LV_INDEV_STATE_PRESSED;
    data->point.x = point.x;
    data->point.y = point.y;
}

}  // namespace

void displayBegin() {
    auto cfg = M5.config();
    // output_power is the 5V rail to the *external Grove ports*, which power
    // our own peripherals -- not a path to the WH Board. It stays on. (The
    // "tester never powers an energetic load" rule in the spec is about the WH
    // Board's output, and is upheld by never driving one.)
    cfg.output_power = true;
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.setColorDepth(16);
    M5.Display.setBrightness(160);
    M5.Display.fillScreen(TFT_BLACK);

    lv_init();

    const size_t buf_px = kHorRes * kBufLines;
    g_buf_a = static_cast<lv_color_t*>(heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA));
    g_buf_b = static_cast<lv_color_t*>(heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA));
    lv_disp_draw_buf_init(&g_draw_buf, g_buf_a, g_buf_b, buf_px);

    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res  = kHorRes;
    g_disp_drv.ver_res  = kVerRes;
    g_disp_drv.flush_cb = flushCb;
    g_disp_drv.draw_buf = &g_draw_buf;
    lv_disp_drv_register(&g_disp_drv);

    lv_indev_drv_init(&g_indev_drv);
    g_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    g_indev_drv.read_cb = touchCb;
    lv_indev_drv_register(&g_indev_drv);

    g_last_tick_ms = millis();
}

void displayService() {
    M5.update();

    const uint32_t now = millis();
    lv_tick_inc(now - g_last_tick_ms);
    g_last_tick_ms = now;

    lv_timer_handler();
}

}  // namespace hal
