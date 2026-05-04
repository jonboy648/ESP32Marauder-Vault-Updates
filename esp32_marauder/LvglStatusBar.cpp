#include "LvglStatusBar.h"

#ifdef HAS_LVGL_UI

// Phase 1 stub. Full implementation creates labels on lv_disp_top_layer
// and updates them on each subsystem callback. Spec section 4.1.

void LvglStatusBar::init()                                  {}
void LvglStatusBar::update_battery(uint8_t pct)             { (void)pct; }
void LvglStatusBar::update_gps(bool has_fix, uint8_t sats)  { (void)has_fix; (void)sats; }
void LvglStatusBar::update_clock(uint8_t h, uint8_t m)      { (void)h; (void)m; }
void LvglStatusBar::update_watchtower_mode(uint8_t mode)    { (void)mode; }

#endif /* HAS_LVGL_UI */
