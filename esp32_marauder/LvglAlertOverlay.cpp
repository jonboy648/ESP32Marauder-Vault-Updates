#include "LvglAlertOverlay.h"

#ifdef HAS_LVGL_UI

#include <lvgl.h>

// CRITICAL alert modal lives on lv_disp_sys_layer so it sits above
// everything including the top-layer status bar (per spec section 4.1).
// It's created hidden and shown only when show_critical() is called.

static lv_obj_t* s_modal      = NULL;
static lv_obj_t* s_reason_lbl = NULL;
static lv_obj_t* s_dismiss    = NULL;
static bool      s_active     = false;

static void dismiss_event_cb(lv_event_t* e) {
    (void)e;
    LvglAlertOverlay::dismiss();
}

void LvglAlertOverlay::init() {
    lv_obj_t* sys = lv_disp_get_layer_sys(NULL);

    s_modal = lv_obj_create(sys);
    lv_obj_set_size(s_modal, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_set_pos(s_modal, 0, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_hex(0x800000), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_modal, 0, 0);

    lv_obj_t* title = lv_label_create(s_modal);
    lv_label_set_text(title, "CRITICAL ALERT");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    s_reason_lbl = lv_label_create(s_modal);
    lv_label_set_long_mode(s_reason_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_reason_lbl, TFT_WIDTH - 16);
    lv_label_set_text(s_reason_lbl, "");
    lv_obj_set_style_text_color(s_reason_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_reason_lbl, LV_ALIGN_CENTER, 0, -10);

    s_dismiss = lv_btn_create(s_modal);
    lv_obj_set_size(s_dismiss, 120, 36);
    lv_obj_align(s_dismiss, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(s_dismiss, dismiss_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* dlbl = lv_label_create(s_dismiss);
    lv_label_set_text(dlbl, "Dismiss");
    lv_obj_center(dlbl);

    // Hidden until triggered.
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void LvglAlertOverlay::show_critical(const char* reason) {
    if (!s_modal) return;
    if (s_reason_lbl) lv_label_set_text(s_reason_lbl, reason ? reason : "(no reason)");
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    s_active = true;
}

void LvglAlertOverlay::dismiss() {
    if (!s_modal) return;
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    s_active = false;
}

bool LvglAlertOverlay::is_active() {
    return s_active;
}

#endif /* HAS_LVGL_UI */
