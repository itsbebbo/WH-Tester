#include "ui/ui.h"

#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "logic/internal_sm.h"
#include "ui/screens.h"
#include "ui/theme.h"

namespace ui {
namespace {

enum class Active : uint8_t { kNone, kHome, kInternal, kExternal };

Active g_active = Active::kNone;

// Leaving a test screen must always put the WH Board back in a safe state, so
// the teardown hook runs from one place rather than from each navigation
// callback.
void leaveCurrent() {
    switch (g_active) {
        case Active::kInternal: internalOnLeave(); break;
        case Active::kExternal: externalOnLeave(); break;
        default: break;
    }
}

}  // namespace

void uiBegin() {
    themeBegin();
    showHome();
}

void showHome() {
    leaveCurrent();
    g_active = Active::kHome;
    lv_scr_load(homeScreen());
}

void showInternal() {
    leaveCurrent();
    g_active = Active::kInternal;
    lv_obj_t* screen = internalScreen();
    internalOnEnter();
    lv_scr_load(screen);
    internalRefresh();
}

void showExternal() {
    leaveCurrent();
    g_active = Active::kExternal;
    lv_obj_t* screen = externalScreen();
    externalOnEnter();
    lv_scr_load(screen);
    externalRefresh();
}

void uiService() {
    switch (g_active) {
        case Active::kInternal:
            // The internal alert is raised on the WH Board's fire status
            // line; the external one on the current threshold, inside
            // externalRefresh(). Both land on the same shared handler.
            if (logic::takeFireEvent()) {
                triggerFireAlert("internal fire status input");
            }
            internalRefresh();
            break;

        case Active::kExternal:
            externalRefresh();
            break;

        default:
            // The state machine keeps running off-screen, but an edge latched
            // while not on the internal screen is discarded rather than
            // replayed on the next visit.
            logic::takeFireEvent();
            break;
    }
}

}  // namespace ui
