#pragma once
#ifndef LvglScreens_h
#define LvglScreens_h

#include "configs.h"

#ifdef HAS_LVGL_UI

#include "MenuFunctions.h"

/**
 * LvglScreens — Phase 1 skeleton.
 *
 * Screen-creation factories. Each `make_*` function builds an LVGL
 * widget tree on a fresh `lv_obj_t* screen`, returns it, and the caller
 * passes it to `lv_scr_load()`. Switching screens is a single
 * `lv_scr_load(target)` call; LVGL handles the transition.
 *
 * Spec: docs/superpowers/specs/2026-05-03-marauder-lvgl-watchtower-design.md §4.2, §9.1
 */
class LvglScreens {
public:
    static void* make_home();                  // 2x3 home grid + Watchtower banner
    static void* make_menu(Menu* menu_def);    // generic menu renderer (reads MenuNode array)
    static void* make_synthesis();             // Watchtower tabbed view (Phase 5+)

    static void show_home();
    static void show_menu(Menu* menu_def);
    static void show_synthesis();
};

#endif /* HAS_LVGL_UI */
#endif /* LvglScreens_h */
