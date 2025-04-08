#include "../ui.h"

static void back_to_screen1(lv_event_t *e) {
    lv_screen_load(ui_Screen1);
}

void ui_Screen2_screen_init(void) {
    const int32_t hor_res = 480;
    const int32_t ver_res = 800;

    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_set_size(ui_Screen2, hor_res, ver_res);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(ui_Screen2, LV_FLEX_FLOW_COLUMN);

    // 标题栏 - 修改为白底黑字
    lv_obj_t *header = lv_obj_create(ui_Screen2);
    lv_obj_set_size(header, hor_res, 50);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFFFFFF), 0);  // 白底
    lv_obj_set_style_border_width(header, 1, 0);                    // 添加底部边框
    lv_obj_set_style_border_color(header, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_hor(header, 10, 0);
    
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);  // 深灰文字
    lv_obj_center(title);

    // 滚动内容区
    lv_obj_t *scroll_container = lv_obj_create(ui_Screen2);
    lv_obj_set_size(scroll_container, hor_res, ver_res - 100);
    lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scroll_container, 10, 0);
    lv_obj_set_style_bg_color(scroll_container, lv_color_white(), 0); // 白底
    lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_ACTIVE);

    // 分辨率选择行
    lv_obj_t *res_row = lv_obj_create(scroll_container);
    lv_obj_remove_style_all(res_row);
    lv_obj_set_size(res_row, hor_res - 20, 50);
    lv_obj_set_flex_flow(res_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(res_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *res_label = lv_label_create(res_row);
    lv_label_set_text(res_label, "Resolution");
    lv_obj_set_style_text_font(res_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(res_label, lv_color_black(), 0);  // 黑字
    
    lv_obj_t *res_dropdown = lv_dropdown_create(res_row);
    lv_dropdown_set_options(res_dropdown, "1080P30\n720P30");
    lv_obj_set_size(res_dropdown, 150, 40);
    lv_obj_set_style_text_font(res_dropdown, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(res_dropdown, lv_color_black(), 0);  // 下拉菜单黑字

    // 开关组
    const char* switches[] = {"IMU Stabilization", "HOT-POINT Tracking", "GNSS Recording"};
    for(int i=0; i<3; i++){
        lv_obj_t *div = lv_line_create(scroll_container);
        static lv_point_t line[] = {{10,0}, {hor_res-10,0}};
        lv_line_set_points(div, line, 2);
        lv_obj_set_style_line_width(div, 1, 0);
        lv_obj_set_style_line_color(div, lv_color_hex(0xDDDDDD), 0);  // 浅灰分割线

        lv_obj_t *sw_row = lv_obj_create(scroll_container);
        lv_obj_remove_style_all(sw_row);
        lv_obj_set_size(sw_row, hor_res - 20, 60);
        lv_obj_set_flex_flow(sw_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(sw_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        lv_obj_t *label = lv_label_create(sw_row);
        lv_label_set_text(label, switches[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x444444), 0);  // 深灰文字
        
        lv_obj_t *sw = lv_switch_create(sw_row);
        lv_obj_set_size(sw, 60, 30);
    }

    // 设备信息板 - 修改为白底黑字
    lv_obj_t *info_panel = lv_obj_create(scroll_container);
    lv_obj_set_size(info_panel, hor_res - 20, 100);
    lv_obj_set_style_bg_color(info_panel, lv_color_white(), 0);  // 白底
    lv_obj_set_style_border_width(info_panel, 1, 0);             // 添加边框
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_pad_all(info_panel, 10, 0);
    lv_obj_set_style_radius(info_panel, 8, 0);
    
    lv_obj_t *info_text = lv_label_create(info_panel);
    lv_label_set_text(info_text, "RK3566 Quad-Core\n"
                                  "Firmware V1.0.0\n"
                                  "Storage: 64GB Free");
    lv_obj_set_style_text_font(info_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_text, lv_color_hex(0x333333), 0);  // 深灰文字
    lv_obj_set_style_text_line_space(info_text, 6, 0);
    lv_obj_center(info_text);

    // 返回按钮
    lv_obj_t *back_btn = lv_button_create(ui_Screen2);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_radius(back_btn, 20, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xFFFFFF), 0);  // 白底按钮
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xDDDDDD), 0);
    lv_obj_add_event_cb(back_btn, back_to_screen1, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(back_label, lv_color_black(), 0);  // 黑字
    lv_obj_center(back_label);
}