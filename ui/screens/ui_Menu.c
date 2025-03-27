#include "../ui.h"

static void back_to_screen1(lv_event_t *e) {
    lv_screen_load(ui_Screen1);
}

void ui_Screen2_screen_init(void) {
    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(0x000000), LV_PART_MAIN);

    // 菜单标题
    lv_obj_t *label = lv_label_create(ui_Screen2);
    lv_label_set_text(label, "Menu");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);

    // 返回按钮
    lv_obj_t *back_btn = lv_button_create(ui_Screen2);
    lv_obj_set_size(back_btn, 100, 50);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(back_btn, back_to_screen1, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
}