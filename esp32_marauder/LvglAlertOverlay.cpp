#include "LvglAlertOverlay.h"

#ifdef HAS_LVGL_UI

// Phase 1 stub. Full implementation creates a hidden modal on
// lv_disp_sys_layer; show_critical() pops it with red styling and
// blocks input until dismissed. Spec section 3.3, 4.1.

static bool s_active = false;

void LvglAlertOverlay::init()                              {}
void LvglAlertOverlay::show_critical(const char* reason)   { (void)reason; s_active = true; }
void LvglAlertOverlay::dismiss()                           { s_active = false; }
bool LvglAlertOverlay::is_active()                         { return s_active; }

#endif /* HAS_LVGL_UI */
