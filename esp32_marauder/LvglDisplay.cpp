#include "LvglDisplay.h"

#ifdef HAS_LVGL_UI

// Phase 1 stub. Full implementation follows in next commit:
//  - lv_init()
//  - lv_disp_drv_register() with TFT_eSPI flush callback
//  - lv_indev_drv_register() bridging Switches button events
//  - periodic lv_task_handler() pump from main loop

static bool s_ready = false;

void LvglDisplay::init() {
    s_ready = true;
}

void LvglDisplay::tick() {
}

bool LvglDisplay::ready() {
    return s_ready;
}

#endif /* HAS_LVGL_UI */
