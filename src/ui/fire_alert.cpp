#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "hal/audio.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace ui {
namespace {

lv_obj_t* g_overlay     = nullptr;
uint32_t  g_last_alert  = 0;

void overlayDoneCb(lv_anim_t* anim) {
    (void)anim;
    if (g_overlay) lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_HIDDEN);
}

void overlayOpaCb(void* obj, int32_t value) {
    lv_obj_set_style_bg_opa(static_cast<lv_obj_t*>(obj),
                            static_cast<lv_opa_t>(value), 0);
}

// The overlay lives on the top layer, so it covers whichever screen is loaded
// and the alert behaviour is defined exactly once.
lv_obj_t* overlay() {
    if (g_overlay) return g_overlay;

    g_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_overlay);
    lv_obj_set_size(g_overlay, kScreenW, kScreenH);
    lv_obj_set_pos(g_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_overlay, kColRed, 0);
    lv_obj_set_style_bg_opa(g_overlay, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_HIDDEN);
    return g_overlay;
}

}  // namespace

void triggerFireAlert(const char* source) {
    const uint32_t now = millis();
    if (g_last_alert != 0 && (now - g_last_alert) < kAlertLockoutMs) return;
    g_last_alert = now;

    Serial.printf("[alert] fire activated (%s)\n", source);

    lv_obj_t* obj = overlay();
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);

    // Snap to full opacity, then fade out over the flash duration.
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_exec_cb(&anim, overlayOpaCb);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&anim, kFlashDurationMs);
    lv_anim_set_ready_cb(&anim, overlayDoneCb);
    lv_anim_start(&anim);

    hal::playBoom();
}

}  // namespace ui
