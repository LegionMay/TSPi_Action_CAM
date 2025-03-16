// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.0
// LVGL version: 9.1.0
// Project name: TSPi_ActionCamera
//ui_Screen1.c

#include "../ui.h"

static void record_btn_event_cb(lv_event_t *e)
{
    printf("start\n");
}

void ui_Screen1_screen_init(void)
{
    // 获取屏幕实际尺寸
    lv_display_t *disp = lv_display_get_default();
    int32_t hor_res = lv_display_get_horizontal_resolution(disp);
    int32_t ver_res = lv_display_get_vertical_resolution(disp);
    
    LV_LOG_USER("DISPLAY: %dx%d", hor_res, ver_res);
    
    // 创建全屏主屏幕
    ui_Screen1 = lv_obj_create(NULL);
    
    // 确保全屏显示 - 移除所有边距和内边距
    lv_obj_set_style_pad_all(ui_Screen1, 0, LV_PART_MAIN);
    lv_obj_set_style_margin_all(ui_Screen1, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_Screen1, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_Screen1, 0, LV_PART_MAIN);
    
    // 设置全屏尺寸
    lv_obj_set_size(ui_Screen1, hor_res, ver_res);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    
    // 设置黑色背景
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), LV_PART_MAIN);
    
    // 创建完全填充屏幕的视频容器
    lv_obj_t *video_container = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(video_container); // 移除所有默认样式
    lv_obj_set_size(video_container, hor_res, ver_res); // 全屏容器
    lv_obj_set_pos(video_container, 0, 0); // 从左上角开始
    lv_obj_set_style_bg_color(video_container, lv_color_hex(0x222222), LV_PART_MAIN);
    
    // 在此容器上分层添加视频预览区域和控件
    
    // 1. 视频预览区域 - 完全填充父容器
    lv_obj_t *video_area = lv_obj_create(video_container);
    lv_obj_remove_style_all(video_area); // 移除所有默认样式
    lv_obj_set_size(video_area, hor_res, ver_res - 30); // 预留底部30px
    lv_obj_set_pos(video_area, 0, 0); // 从顶部开始
    lv_obj_set_style_bg_color(video_area, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_border_width(video_area, 0, LV_PART_MAIN); // 无边框
    
    // 视频区域提示文本
    lv_obj_t *hint_label = lv_label_create(video_area);
    lv_label_set_text_fmt(hint_label, "Video(%dx%d)", hor_res, ver_res - 30);
    lv_obj_center(hint_label);
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x888888), LV_PART_MAIN);
    
    // 2. 底部半透明控制栏 - 覆盖在视频区域底部
    lv_obj_t *bottom_bar = lv_obj_create(video_container);
    lv_obj_remove_style_all(bottom_bar);
    lv_obj_set_size(bottom_bar, hor_res, 30);
    lv_obj_set_pos(bottom_bar, 0, ver_res - 30); // 位于底部
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_50, LV_PART_MAIN); // 半透明
    
    // "ActionCAM"文本
    lv_obj_t *brand_label = lv_label_create(bottom_bar);
    lv_label_set_text(brand_label, "ActionCAM");
    lv_obj_align(brand_label, LV_ALIGN_CENTER, 0, 0); // 在底部栏中央
    lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(brand_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    // 3. 录制按钮 - 位于右侧，悬浮在视频区域上
    lv_obj_t *record_btn = lv_button_create(video_container);
    lv_obj_set_size(record_btn, 60, 60);
    lv_obj_align(record_btn, LV_ALIGN_RIGHT_MID, -20, -15); // 右侧中间，略微上移
    lv_obj_set_style_radius(record_btn, 30, LV_PART_MAIN); // 完全圆形
    lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    
    // 录制按钮文字
    lv_obj_t *record_label = lv_label_create(record_btn);
    lv_label_set_text(record_label, "REC");
    lv_obj_center(record_label);
    lv_obj_set_style_text_color(record_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(record_label, &lv_font_montserrat_16, LV_PART_MAIN);
    
    // 添加录制按钮点击事件回调
    lv_obj_add_event_cb(record_btn, record_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    // 确保全屏屏幕直接加载
    lv_scr_load(ui_Screen1);
}