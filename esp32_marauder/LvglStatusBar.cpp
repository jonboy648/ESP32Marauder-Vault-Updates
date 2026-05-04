#include "LvglStatusBar.h"

#ifdef HAS_LVGL_UI

#include <lvgl.h>

// All status-bar widgets live on lv_disp_top_layer so they sit above
// every screen the user navigates to. They never get cleared by
// lv_scr_load(), which is the whole point of the top layer.

static lv_obj_t* s_clock_lbl     = NULL;
static lv_obj_t* s_gps_lbl       = NULL;
static lv_obj_t* s_battery_lbl   = NULL;
static lv_obj_t* s_wt_mode_lbl   = NULL;

void LvglStatusBar::init() {
    lv_obj_t* top = lv_disp_get_layer_top(NULL);

    // 14px tall translucent bar across the full width.
    lv_obj_t* bar = lv_obj_create(top);
    lv_obj_set_size(bar, TFT_WIDTH, 14);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 1, 0);

    s_clock_lbl = lv_label_create(bar);
    lv_label_set_text(s_clock_lbl, "--:--");
    lv_obj_set_style_text_color(s_clock_lbl, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(s_clock_lbl, LV_ALIGN_LEFT_MID, 2, 0);

    s_gps_lbl = lv_label_create(bar);
    lv_label_set_text(s_gps_lbl, "GPS-");
    lv_obj_set_style_text_color(s_gps_lbl, lv_color_hex(0x808080), 0);
    lv_obj_align(s_gps_lbl, LV_ALIGN_LEFT_MID, 60, 0);

    s_battery_lbl = lv_label_create(bar);
    lv_label_set_text(s_battery_lbl, "--%");
    lv_obj_set_style_text_color(s_battery_lbl, lv_color_hex(0xC0C0C0), 0);
    lv_obj_align(s_battery_lbl, LV_ALIGN_RIGHT_MID, -4, 0);

    s_wt_mode_lbl = lv_label_create(bar);
    lv_label_set_text(s_wt_mode_lbl, "OFF");
    lv_obj_set_style_text_color(s_wt_mode_lbl, lv_color_hex(0x606060), 0);
    lv_obj_align(s_wt_mode_lbl, LV_ALIGN_RIGHT_MID, -50, 0);
}

void LvglStatusBar::update_battery(uint8_t pct) {
    if (!s_battery_lbl) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
    lv_label_set_text(s_battery_lbl, buf);
}

void LvglStatusBar::update_gps(bool has_fix, uint8_t sats) {
    if (!s_gps_lbl) return;
    char buf[16];
    if (has_fix) {
        snprintf(buf, sizeof(buf), "GPS:%u", (unsigned)sats);
        lv_obj_set_style_text_color(s_gps_lbl, lv_color_hex(0x40FF40), 0);
    } else {
        snprintf(buf, sizeof(buf), "GPS-");
        lv_obj_set_style_text_color(s_gps_lbl, lv_color_hex(0x808080), 0);
    }
    lv_label_set_text(s_gps_lbl, buf);
}

void LvglStatusBar::update_clock(uint8_t h, uint8_t m) {
    if (!s_clock_lbl) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)h, (unsigned)m);
    lv_label_set_text(s_clock_lbl, buf);
}

void LvglStatusBar::update_watchtower_mode(uint8_t mode) {
    if (!s_wt_mode_lbl) return;
    static const char* names[3] = { "OFF", "ACTIVE", "PAUSED" };
    static const uint32_t colors[3] = { 0x606060, 0x40FF40, 0xFFC040 };
    if (mode > 2) mode = 0;
    lv_label_set_text(s_wt_mode_lbl, names[mode]);
    lv_obj_set_style_text_color(s_wt_mode_lbl, lv_color_hex(colors[mode]), 0);
}

#endif /* HAS_LVGL_UI */
