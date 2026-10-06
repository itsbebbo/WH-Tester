// Screen ownership and navigation.
#pragma once

namespace ui {

void uiBegin();

// Navigation. Screens are built once and cached; switching only reloads them.
void showHome();
void showInternal();
void showExternal();

// Per-frame refresh of whichever screen is loaded.
void uiService();

// Shared fire-activated alert (spec section 9): full-screen red flash plus the
// baked-in explosion sample. Safe to call from either test screen; repeat
// calls inside the lockout window are ignored.
void triggerFireAlert(const char* source);

}  // namespace ui
