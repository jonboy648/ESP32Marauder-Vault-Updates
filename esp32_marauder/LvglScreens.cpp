#include "LvglScreens.h"

#ifdef HAS_LVGL_UI

#include <lvgl.h>

// 2x3 home grid colors per spec section 4.2.
// Hex values match the existing TFT_eSPI tile colors used by Display.cpp.
struct HomeTile {
    const char* label;
    uint32_t    color;
};

static const HomeTile s_home_tiles[6] = {
    { "WiFi",       0x00FF00 }, // TFTGREEN
    { "Bluetooth",  0x00FFFF }, // TFTCYAN
    { "GPS",        0xFF0000 }, // TFTRED
    { "Watchtower", 0xFF00FF }, // TFTMAGENTA — new tile per spec
    { "Device",     0x0080FF }, // TFTBLUE
    { "Reboot",     0xC0C0C0 }, // TFTLIGHTGREY
};

// Phase 1: home tiles render but their callbacks are stubs.
// Phase 1 follow-up will wire them to the corresponding Menu* via MenuFunctions.
static void home_tile_event_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    (void)idx;
    // TODO: dispatch to menu_function_obj.changeMenu() based on idx
}

void* LvglScreens::make_home() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // 2x3 grid: 2 columns, 3 rows. Status bar reserves 14px at top; tiles fill below.
    static const int kCols = 2, kRows = 3;
    static const int kTopMargin = 14;
    const int tile_w = TFT_WIDTH / kCols;
    const int tile_h = (TFT_HEIGHT - kTopMargin) / kRows;

    for (int i = 0; i < 6; i++) {
        const HomeTile& t = s_home_tiles[i];
        int col = i % kCols;
        int row = i / kCols;

        lv_obj_t* btn = lv_btn_create(scr);
        lv_obj_set_size(btn, tile_w - 4, tile_h - 4);
        lv_obj_set_pos(btn, col * tile_w + 2, kTopMargin + row * tile_h + 2);
        lv_obj_set_style_bg_color(btn, lv_color_hex(t.color), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_event_cb(btn, home_tile_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, t.label);
        lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
        lv_obj_center(label);
    }

    return scr;
}

// Click event for menu items: invokes the MenuNode's stored callable.
static void menu_item_event_cb(lv_event_t* e) {
    MenuNode* node = (MenuNode*)lv_event_get_user_data(e);
    if (node && node->callable) node->callable();
}

void* LvglScreens::make_menu(Menu* menu_def) {
    if (!menu_def || !menu_def->list) return nullptr;

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Title bar: 14px reserved for status bar, then a label with menu name.
    static const int kTitleY = 14;
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, menu_def->name.c_str());
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, kTitleY);

    // Scrollable list of menu items, fills the screen below the title.
    lv_obj_t* list = lv_list_create(scr);
    lv_obj_set_size(list, TFT_WIDTH, TFT_HEIGHT - kTitleY - 18);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, kTitleY + 18);

    int n = menu_def->list->size();
    for (int i = 0; i < n; i++) {
        MenuNode node = menu_def->list->get(i);
        lv_obj_t* btn = lv_list_add_btn(list, NULL, node.name.c_str());
        // Pass a pointer to the stored MenuNode so the callback can fire its callable.
        // We use the LinkedList's internal storage; lifetime matches the menu.
        lv_obj_add_event_cb(btn, menu_item_event_cb, LV_EVENT_CLICKED,
                            &menu_def->list->get(i));
    }

    return scr;
}
void* LvglScreens::make_synthesis()             { return nullptr; }

void LvglScreens::show_home() {
    lv_obj_t* scr = (lv_obj_t*)make_home();
    if (scr) lv_scr_load(scr);
}
void LvglScreens::show_menu(Menu* menu_def) {
    lv_obj_t* scr = (lv_obj_t*)make_menu(menu_def);
    if (scr) lv_scr_load(scr);
}
void LvglScreens::show_synthesis()              {}

#endif /* HAS_LVGL_UI */
