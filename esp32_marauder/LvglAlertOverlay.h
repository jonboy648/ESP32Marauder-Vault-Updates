#pragma once
#ifndef LvglAlertOverlay_h
#define LvglAlertOverlay_h

#include "configs.h"

#ifdef HAS_LVGL_UI

/**
 * LvglAlertOverlay — Phase 1 skeleton.
 *
 * System-layer modal used only by CRITICAL Watchtower alerts (per spec
 * §3.3). Drawn on `lv_disp_sys_layer` so it sits above everything,
 * including menus. Hard-blocks input until the user acknowledges.
 *
 * Phase 1 lands the API and the overlay primitive. Real CRITICAL alert
 * triggering happens in Phase 3 (classification rules).
 *
 * Spec: docs/superpowers/specs/2026-05-03-marauder-lvgl-watchtower-design.md §3.3, §4.1
 */
class LvglAlertOverlay {
public:
    static void init();                              // create hidden overlay on sys layer
    static void show_critical(const char* reason);   // pop modal; blocks input until dismiss
    static void dismiss();                           // hide overlay
    static bool is_active();
};

#endif /* HAS_LVGL_UI */
#endif /* LvglAlertOverlay_h */
