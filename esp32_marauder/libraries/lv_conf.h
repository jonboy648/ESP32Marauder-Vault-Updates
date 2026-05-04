/**
 * @file lv_conf.h
 * Marauder-tuned LVGL v8.3.11 configuration.
 * Phase 1 stub — minimal enables only. Will grow per spec section 4.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color depth: 16-bit for ILI9341 RGB565 */
#define LV_COLOR_DEPTH      16
#define LV_COLOR_16_SWAP    1

/* Memory: 32 KB internal pool */
#define LV_MEM_CUSTOM       0
#define LV_MEM_SIZE         (32U * 1024U)

/* Tick: provided by Arduino millis() — see LvglDisplay.cpp */
#define LV_TICK_CUSTOM              1
#define LV_TICK_CUSTOM_INCLUDE      "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Display refresh */
#define LV_DISP_DEF_REFR_PERIOD     30
#define LV_INDEV_DEF_READ_PERIOD    30

/* No filesystem, no logging, no asserts in this stub */
#define LV_USE_LOG          0
#define LV_USE_ASSERT_NULL  0

#endif /* LV_CONF_H */
