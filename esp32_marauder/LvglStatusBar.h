#pragma once
#ifndef LvglStatusBar_h
#define LvglStatusBar_h

#include "configs.h"
#include <stdint.h>

#ifdef HAS_LVGL_UI

/**
 * LvglStatusBar — Phase 1 skeleton.
 *
 * The persistent 12px-tall status bar drawn on LVGL's top layer
 * (`lv_disp_top_layer`). Always visible regardless of which screen is
 * loaded on the base layer. Shows time, GPS fix, battery %, RF activity
 * indicator, current Watchtower mode (ACTIVE/PAUSED/OFF — Phase 2+).
 *
 * Spec: docs/superpowers/specs/2026-05-03-marauder-lvgl-watchtower-design.md §4.1, §4.2
 */
class LvglStatusBar {
public:
    static void init();                 // create widgets on top layer
    static void update_battery(uint8_t pct);
    static void update_gps(bool has_fix, uint8_t sat_count);
    static void update_clock(uint8_t h, uint8_t m);
    static void update_watchtower_mode(uint8_t mode);  // 0=OFF 1=ACTIVE 2=PAUSED
};

#endif /* HAS_LVGL_UI */
#endif /* LvglStatusBar_h */
