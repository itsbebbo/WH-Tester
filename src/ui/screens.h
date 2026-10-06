// Internal interface between ui.cpp and the individual screen modules.
// Each screen builds itself once and exposes a refresh hook driven by uiService().
#pragma once

#include <lvgl.h>

namespace ui {

lv_obj_t* homeScreen();
lv_obj_t* internalScreen();
lv_obj_t* externalScreen();

void internalRefresh();
void externalRefresh();

// Called when a screen is entered, so it can seed its edge detectors against
// the present state -- arriving on a screen must never itself raise an alert.
void internalOnEnter();
void externalOnEnter();

// Called when a screen is left, so it can drop the WH Board into a safe state.
void internalOnLeave();
void externalOnLeave();

}  // namespace ui
