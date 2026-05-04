#include "LvglScreens.h"

#ifdef HAS_LVGL_UI

// Phase 1 stub. Full implementation builds widget trees:
//  - make_home(): 2x3 grid + Watchtower banner (spec section 4.2)
//  - make_menu(): renders MenuNode array as LVGL list (spec section 4.5)
//  - make_synthesis(): tabbed Map/List/Detail/Settings (spec section 4.3)

void* LvglScreens::make_home()                  { return nullptr; }
void* LvglScreens::make_menu(Menu* menu_def)    { (void)menu_def; return nullptr; }
void* LvglScreens::make_synthesis()             { return nullptr; }

void LvglScreens::show_home()                   {}
void LvglScreens::show_menu(Menu* menu_def)     { (void)menu_def; }
void LvglScreens::show_synthesis()              {}

#endif /* HAS_LVGL_UI */
