#include "../ui.h"

static void back_to_screen1(lv_event_t *e) {
    lv_screen_load(ui_Screen1);
}

void ui_Screen2_screen_init(void) {
    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_flex_flow(ui_Screen2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Screen2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 菜单标题
    lv_obj_t *label = lv_label_create(ui_Screen2);
    lv_label_set_text(label, "Settings Menu");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_bottom(label, 20, 0);

    // 主设置容器
    lv_obj_t *settings_container = lv_obj_create(ui_Screen2);
    lv_obj_remove_style_all(settings_container);
    lv_obj_set_size(settings_container, lv_pct(90), lv_pct(80));
    lv_obj_set_flex_flow(settings_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_AROUND);
    lv_obj_set_style_pad_all(settings_container, 10, 0);
    lv_obj_set_style_pad_gap(settings_container, 15, 0);

    // 视频参数设置
    lv_obj_t *res_dropdown = lv_dropdown_create(settings_container);
    lv_dropdown_set_options(res_dropdown, "1080P30FPS\n720P60FPS\n720P30FPS");
    lv_obj_set_width(res_dropdown, 200);
    lv_obj_add_flag(res_dropdown, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_bg_color(res_dropdown, lv_color_hex(0x333333), 0);

    // 功能开关容器
    lv_obj_t *create_switch_row(const char *text) {
        lv_obj_t *row = lv_obj_create(settings_container);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), 40);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_set_size(sw, 60, 30);
        return row;
    }

    create_switch_row("IMU Stabilization");
    create_switch_row("HOT-POINT Tracking");
    create_switch_row("GNSS Recording");

    // 设备信息显示
    lv_obj_t *info_panel = lv_obj_create(settings_container);
    lv_obj_remove_style_all(info_panel);
    lv_obj_set_size(info_panel, lv_pct(100), 80);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_pad_all(info_panel, 10, 0);

    lv_obj_t *info_label = lv_label_create(info_panel);
    lv_label_set_text(info_label, "RK3566 | FW V1.0.0\nStorage: 64GB Free");
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);

    // 返回按钮
    lv_obj_t *back_btn = lv_button_create(ui_Screen2);
    lv_obj_set_size(back_btn, 120, 50);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(back_btn, back_to_screen1, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x444444), 0);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
}