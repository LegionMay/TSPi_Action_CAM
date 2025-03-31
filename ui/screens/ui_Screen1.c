#include <lvgl/lvgl.h>
#include "../ui.h"
#include "ui/sdcard/sd_status.h"  // 添加SD卡状态模块头文件

#include <stdio.h>
#include <sys/shm.h>
#include <string.h>

#define SHM_KEY 1234
#define SHM_SIZE 800 * 480 * 4  

static void record_btn_event_cb(lv_event_t *e) {
    static uint8_t is_recording = 0;
    lv_obj_t *record_btn = lv_event_get_target(e);
    lv_obj_t *record_label = lv_obj_get_child(record_btn, 0);
    
    if (is_recording) {
        printf("Recording stopped\n");
        lv_label_set_text(record_label, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
    } else {
        printf("Recording started\n");
        lv_label_set_text(record_label, LV_SYMBOL_STOP);
        lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xCC0000), LV_PART_MAIN);
    }
    is_recording = !is_recording;
}

static void menu_btn_event_cb(lv_event_t *e) {
    lv_screen_load(ui_Screen2);
}

static void update_video_preview(lv_timer_t *timer) {
    lv_obj_t *video_area = (lv_obj_t *)lv_timer_get_user_data(timer);
    static void *shm_ptr = NULL;

    if (!shm_ptr) {
        int shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
        if (shmid == -1) return;
        shm_ptr = shmat(shmid, NULL, 0);
        if (shm_ptr == (void *)-1) return;
    }

    lv_image_set_src(video_area, shm_ptr);
    lv_obj_invalidate(video_area);
}

// 自定义SD卡显示回调
static void custom_sd_display(lv_obj_t *label, uint8_t percent) {
    lv_label_set_text_fmt(label, "%d%%", percent);
    lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    
    // 超过90%显示红色
    if(percent > 90) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), 0);
    }
}

void ui_Screen1_screen_init(void) {
    lv_display_t *disp = lv_display_get_default();
    const int32_t hor_res = 480;
    const int32_t ver_res = 800;

    // 创建主容器
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_Screen1, hor_res, ver_res);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), 0);

    // 顶部状态栏
    lv_obj_t *status_bar = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, hor_res, 35);
    lv_obj_set_style_pad_hor(status_bar, 15, 0);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, 
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // SD卡状态组件
    lv_obj_t *sd_container = lv_obj_create(status_bar);
    lv_obj_remove_style_all(sd_container);
    lv_obj_set_flex_flow(sd_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sd_container, LV_FLEX_ALIGN_CENTER, 
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // SD卡图标
    lv_obj_t *sd_icon = lv_label_create(sd_container);
    lv_label_set_text(sd_icon, LV_SYMBOL_SD_CARD);
    lv_obj_set_style_text_color(sd_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(sd_icon, &lv_font_montserrat_16, 0);
    
    // 初始化SD卡状态模块
    sd_status_init(sd_container);  // 传入父容器
    sd_set_display_callback(custom_sd_display);

    // 中间左侧：录制模式
    lv_obj_t *rec_mode = lv_label_create(status_bar);
    lv_label_set_text(rec_mode, "1080P30");
    lv_obj_set_style_text_color(rec_mode, lv_color_white(), 0);
    lv_obj_set_style_text_font(rec_mode, &lv_font_montserrat_16, 0);

    // 中间右侧：GNSS卫星信息
    lv_obj_t *gnss_info = lv_obj_create(status_bar);
    lv_obj_remove_style_all(gnss_info);
    lv_obj_set_flex_flow(gnss_info, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gnss_info, LV_FLEX_ALIGN_CENTER, 
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *gnss_icon = lv_label_create(gnss_info);
    lv_label_set_text(gnss_icon, LV_SYMBOL_GPS);
    lv_obj_set_style_text_color(gnss_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(gnss_icon, &lv_font_montserrat_16, 0);
    
    lv_obj_t *gnss_label = lv_label_create(gnss_info);
    lv_label_set_text(gnss_label, "0");
    lv_obj_set_style_text_color(gnss_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(gnss_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_left(gnss_label, 5, 0);

    // 右侧：电池信息
    lv_obj_t *battery_info = lv_obj_create(status_bar);
    lv_obj_remove_style_all(battery_info);
    lv_obj_set_flex_flow(battery_info, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(battery_info, LV_FLEX_ALIGN_CENTER, 
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *bat_label = lv_label_create(battery_info);
    lv_label_set_text(bat_label, "80%");
    lv_obj_set_style_text_color(bat_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_14, 0);
    
    lv_obj_t *bat_icon = lv_label_create(battery_info);
    lv_label_set_text(bat_icon, LV_SYMBOL_BATTERY_3);
    lv_obj_set_style_text_color(bat_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(bat_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_left(bat_icon, 8, 0);

    // 视频预览容器
    lv_obj_t *video_container = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(video_container);
    lv_obj_set_size(video_container, hor_res, ver_res - 100);
    lv_obj_align(video_container, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(video_container, lv_color_hex(0x222222), 0);

    // 视频预览区域
    lv_obj_t *video_area = lv_image_create(video_container);
    lv_obj_set_size(video_area, 480, 640);
    lv_obj_center(video_area);
    lv_obj_set_style_bg_color(video_area, lv_color_hex(0x333333), 0);

    // 底部控制栏
    lv_obj_t *bottom_bar = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(bottom_bar);
    lv_obj_set_size(bottom_bar, hor_res, 70);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, 
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(bottom_bar, 20, 0);

    // 左下角预览按钮
    lv_obj_t *preview_btn = lv_button_create(bottom_bar);
    lv_obj_set_size(preview_btn, 50, 50);
    lv_obj_set_style_radius(preview_btn, 25, 0);
    lv_obj_set_style_bg_color(preview_btn, lv_color_hex(0x444444), 0);
    
    lv_obj_t *preview_icon = lv_label_create(preview_btn);
    lv_label_set_text(preview_icon, LV_SYMBOL_VIDEO);
    lv_obj_center(preview_icon);
    lv_obj_set_style_text_color(preview_icon, lv_color_white(), 0);

    // 中间录制按钮
    lv_obj_t *record_btn = lv_button_create(ui_Screen1);
    lv_obj_set_size(record_btn, 80, 80);
    lv_obj_align(record_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_radius(record_btn, 40, 0);
    lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xFF0000), 0);
    lv_obj_add_event_cb(record_btn, record_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *record_icon = lv_label_create(record_btn);
    lv_label_set_text(record_icon, LV_SYMBOL_PLAY);
    lv_obj_center(record_icon);
    lv_obj_set_style_text_color(record_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(record_icon, &lv_font_montserrat_24, 0);

    // 右下角设置按钮
    lv_obj_t *menu_btn = lv_button_create(bottom_bar);
    lv_obj_set_size(menu_btn, 50, 50);
    lv_obj_set_style_radius(menu_btn, 25, 0);
    lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(menu_btn, menu_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *menu_icon = lv_label_create(menu_btn);
    lv_label_set_text(menu_icon, LV_SYMBOL_SETTINGS);
    lv_obj_center(menu_icon);
    lv_obj_set_style_text_color(menu_icon, lv_color_white(), 0);

    // 视频更新定时器
    lv_timer_create(update_video_preview, 33, video_area);
    lv_screen_load(ui_Screen1);
}