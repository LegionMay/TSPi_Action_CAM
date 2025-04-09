#include "../ui.h"
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSG_KEY 9876
#define GNSS_FIFO_PATH "/tmp/gnss_control_fifo"

static int msgid;

// Message structure for System V
typedef struct {
    long mtype;         // Message type (required by System V)
    char mtext[32];     // Message content
} MsgBuf;

// Initialize System V message queue
static void init_message_queue(void) {
    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget failed in UI Screen2 process");
        exit(1);
    }
}

// Resolution dropdown event callback
static void res_dropdown_event_cb(lv_event_t *e) {
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    MsgBuf msg;
    msg.mtype = 1;  // Use type 1 for all messages
    int resolution = (selected == 0) ? 1080 : 720;
    snprintf(msg.mtext, sizeof(msg.mtext), "resolution %d", resolution);
    if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("msgsnd failed for resolution control");
    }
    printf("Resolution selected: %dp\n", resolution);
}

// GNSS switch event callback
static void gnss_switch_event_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    int is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    // 打开GNSS控制FIFO
    int fd = open(GNSS_FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fd >= 0) {
        const char *cmd = is_on ? "start" : "stop";
        write(fd, cmd, strlen(cmd));
        close(fd);
        printf("Sent GNSS command: %s\n", cmd);
    } else {
        perror("Failed to open GNSS control FIFO");
        printf("Is GNSS process running? FIFO path: %s\n", GNSS_FIFO_PATH);
    }
}

static void back_to_screen1(lv_event_t *e) {
    lv_screen_load(ui_Screen1);
}

void ui_Screen2_screen_init(void) {
    const int32_t hor_res = 480;
    const int32_t ver_res = 800;

    init_message_queue();

    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_set_size(ui_Screen2, hor_res, ver_res);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(0x000000), 0);
    lv_obj_set_flex_flow(ui_Screen2, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *header = lv_obj_create(ui_Screen2);
    lv_obj_set_size(header, hor_res, 50);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_hor(header, 10, 0);
    
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_center(title);

    lv_obj_t *scroll_container = lv_obj_create(ui_Screen2);
    lv_obj_set_size(scroll_container, hor_res, ver_res - 100);
    lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scroll_container, 10, 0);
    lv_obj_set_style_bg_color(scroll_container, lv_color_white(), 0);
    lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_ACTIVE);

    lv_obj_t *res_row = lv_obj_create(scroll_container);
    lv_obj_remove_style_all(res_row);
    lv_obj_set_size(res_row, hor_res - 20, 50);
    lv_obj_set_flex_flow(res_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(res_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *res_label = lv_label_create(res_row);
    lv_label_set_text(res_label, "Resolution");
    lv_obj_set_style_text_font(res_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(res_label, lv_color_black(), 0);
    
    lv_obj_t *res_dropdown = lv_dropdown_create(res_row);
    lv_dropdown_set_options(res_dropdown, "1080P30\n720P30");
    lv_obj_set_size(res_dropdown, 150, 40);
    lv_obj_set_style_text_font(res_dropdown, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(res_dropdown, lv_color_black(), 0);
    lv_obj_add_event_cb(res_dropdown, res_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    const char* switches[] = {"IMU Stabilization", "HOT-POINT Tracking", "GNSS Recording"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *div = lv_line_create(scroll_container);
        // 修改为 lv_point_precise_t 类型
        static lv_point_precise_t line_points[] = {{10,0}, {hor_res-10,0}};
        lv_line_set_points(div, line_points, 2);
        lv_obj_set_style_line_width(div, 1, 0);
        lv_obj_set_style_line_color(div, lv_color_hex(0xDDDDDD), 0);

        lv_obj_t *sw_row = lv_obj_create(scroll_container);
        lv_obj_remove_style_all(sw_row);
        lv_obj_set_size(sw_row, hor_res - 20, 60);
        lv_obj_set_flex_flow(sw_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(sw_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        lv_obj_t *label = lv_label_create(sw_row);
        lv_label_set_text(label, switches[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x444444), 0);
        
        lv_obj_t *sw = lv_switch_create(sw_row);
        lv_obj_set_size(sw, 60, 30);
        
        // 为GNSS开关设置事件回调
        if (i == 2) { // GNSS Recording开关
            lv_obj_add_event_cb(sw, gnss_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }
    }

    lv_obj_t *info_panel = lv_obj_create(scroll_container);
    lv_obj_set_size(info_panel, hor_res - 20, 100);
    lv_obj_set_style_bg_color(info_panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(info_panel, 1, 0);
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_pad_all(info_panel, 10, 0);
    lv_obj_set_style_radius(info_panel, 8, 0);
    
    lv_obj_t *info_text = lv_label_create(info_panel);
    lv_label_set_text(info_text, "RK3566 Quad-Core\nFirmware V1.0.0\nStorage: 32GB");
    lv_obj_set_style_text_font(info_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_text, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_line_space(info_text, 6, 0);
    lv_obj_center(info_text);

    lv_obj_t *back_btn = lv_button_create(ui_Screen2);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_radius(back_btn, 20, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xDDDDDD), 0);
    lv_obj_add_event_cb(back_btn, back_to_screen1, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(back_label, lv_color_black(), 0);
    lv_obj_center(back_label);
}