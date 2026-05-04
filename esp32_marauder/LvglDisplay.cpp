#include "LvglDisplay.h"

#ifdef HAS_LVGL_UI

#include <lvgl.h>
#include "Display.h"
#include "LvglScreens.h"
#include "LvglStatusBar.h"
#include "LvglAlertOverlay.h"

extern Display display_obj;

// Single-buffer, ~10 lines deep. ~6 KB for 320x240 RGB565.
// Full-frame double buffer would need ~150 KB (PSRAM). Phase 1 keeps it simple.
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t         s_buf1[TFT_WIDTH * 10];
static lv_disp_drv_t      s_disp_drv;
static lv_indev_drv_t     s_indev_drv;
static bool               s_ready = false;

// LVGL flush callback: push the pixel area to TFT_eSPI.
static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    display_obj.tft.startWrite();
    display_obj.tft.setAddrWindow(area->x1, area->y1, w, h);
    display_obj.tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    display_obj.tft.endWrite();

    lv_disp_flush_ready(drv);
}

// LVGL touch input callback. V6.1 is touch-primary: a single C button plus a
// resistive ILI9341 touchscreen. We bridge the touchscreen as an LV_INDEV_TYPE_POINTER.
// Spec section 4.5 originally said "4-button layout"; that was incorrect for V6.1
// hardware. Buttons can be added later as a secondary keypad indev for boards with them.
static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    uint16_t x = 0, y = 0;
    if (display_obj.updateTouch(&x, &y, 600)) {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void LvglDisplay::init() {
    lv_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, NULL, TFT_WIDTH * 10);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = TFT_WIDTH;
    s_disp_drv.ver_res  = TFT_HEIGHT;
    s_disp_drv.flush_cb = flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    s_ready = true;

    LvglStatusBar::init();
    LvglAlertOverlay::init();
    LvglScreens::show_home();
}

void LvglDisplay::tick() {
    if (s_ready) lv_task_handler();
}

bool LvglDisplay::ready() {
    return s_ready;
}

#endif /* HAS_LVGL_UI */
