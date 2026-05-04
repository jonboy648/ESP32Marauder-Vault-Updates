#pragma once
#ifndef LvglDisplay_h
#define LvglDisplay_h

#include "configs.h"

#ifdef HAS_LVGL_UI

/**
 * LvglDisplay — Phase 1 skeleton.
 *
 * Owns LVGL initialization, display driver registration, input device
 * registration, and the periodic lv_task_handler() pump. Wraps the
 * existing TFT_eSPI instance from Display.cpp; does not replace it.
 *
 * Spec: docs/superpowers/specs/2026-05-03-marauder-lvgl-watchtower-design.md §4.1, §9.1
 */
class LvglDisplay {
public:
    static void init();        // initialize LVGL stack, register display + input
    static void tick();         // call from main loop; drives lv_task_handler
    static bool ready();        // true once init() has completed
};

#endif /* HAS_LVGL_UI */
#endif /* LvglDisplay_h */
