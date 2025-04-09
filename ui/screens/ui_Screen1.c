/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/lv_port_linux/ui/screens/ui_Screen1.c
 */
#include <lvgl/lvgl.h>
#include "../ui.h"
#include "ui/sdcard/sd_status.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>

// Shared memory and semaphore definitions
#define SHM_KEY 1234
#define SHM_SIZE 800 * 450 * 4
#define RECORD_SHM_KEY 5678
#define GNSS_FIFO_PATH "/tmp/gnss_ui_fifo"

// 进程启动路径
#define GNSS_PROCESS "/app/GNSS/gnss_collector"
#define IMU_PROCESS "/app/imu/imu_logger"
#define WEB_PROCESS "/app/web/civetweb -document_root /mnt/sdcard -listening_ports 8080 -enable_directory_listing yes -index_files \"\" -title \"File List\" -extra_mime_types .mkv=video/x-matroska"

// 进程PID
static pid_t gnss_pid = -1;
static pid_t imu_pid = -1;

// Semaphore pointer
static sem_t *sem;

// Shared memory structure for recording control
typedef struct {
    int is_recording;  // 0: not recording, 1: recording
} RecordControl;

static RecordControl *record_control = NULL;

// Image descriptor for LVGL
static lv_image_dsc_t video_img_dsc = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .w = 450,
        .h = 800,
    },
    .data_size = 800 * 450 * 4,
    .data = NULL,
};

// Initialize named semaphore
static void init_semaphore(void) {
    sem = sem_open("/preview_sem", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open failed in UI process");
        exit(1);
    }
}

// Initialize recording control shared memory
static void init_record_control(void) {
    int shmid = shmget(RECORD_SHM_KEY, sizeof(RecordControl), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed for record control in UI process");
        exit(1);
    }
    record_control = (RecordControl *)shmat(shmid, NULL, 0);
    if (record_control == (void *)-1) {
        perror("shmat failed for record control in UI process");
        exit(1);
    }
    record_control->is_recording = 0;
}

// Initialize GNSS FIFO for reading
static void init_gnss_fifo(void) {
    // 确保FIFO存在
    if (access(GNSS_FIFO_PATH, F_OK) != 0) {
        // 如果不存在则创建
        if (mkfifo(GNSS_FIFO_PATH, 0666) < 0) {
            perror("Failed to create GNSS UI FIFO");
        } else {
            printf("Created GNSS UI FIFO: %s\n", GNSS_FIFO_PATH);
        }
    }
}

// 启动进程函数
static pid_t start_process(const char* command) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork failed");
        return -1;
    } 
    else if (pid == 0) {
        // 子进程执行命令
        char *args[20];
        char cmd_copy[256];
        strcpy(cmd_copy, command);
        
        // 分解命令行参数
        int i = 0;
        args[i] = strtok(cmd_copy, " ");
        while (args[i] != NULL && i < 19) {
            i++;
            args[i] = strtok(NULL, " ");
        }
        
        // 执行命令
        execv(args[0], args);
        
        // 如果execv返回，说明出错了
        perror("execv failed");
        exit(1);
    }
    
    // 父进程返回子进程PID
    return pid;
}

// 终止进程函数
static void stop_process(pid_t pid) {
    if (pid > 0) {
        // 发送SIGTERM信号终止进程
        if (kill(pid, SIGTERM) != 0) {
            // 如果失败，尝试SIGKILL
            kill(pid, SIGKILL);
        }
    }
}

// Back button event callback
static void back_btn_event_cb(lv_event_t *e) {
    lv_obj_t *back_btn = lv_event_get_target(e);
    lv_obj_t *container = lv_obj_get_parent(back_btn);
    lv_obj_delete(container);
}

// Record button state
static uint8_t is_recording = 0;
static time_t start_time;
static lv_obj_t *record_btn = NULL;

// Function to trigger recording action (can be called programmatically)
static void trigger_record_button(int start_recording) {
    if (!record_btn) {
        printf("Record button not initialized\n");
        return;
    }
    
    lv_obj_t *record_label = lv_obj_get_child(record_btn, 0);
    
    // Don't trigger if already in the requested state
    if ((start_recording && is_recording) || (!start_recording && !is_recording)) {
        return;
    }
    
    if (start_recording) {
        printf("Recording started\n");
        lv_label_set_text(record_label, LV_SYMBOL_STOP);
        lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xCC0000), LV_PART_MAIN);
        record_control->is_recording = 1;
        start_time = time(NULL);
        is_recording = 1;
        
        // 启动GNSS和IMU进程
        gnss_pid = start_process(GNSS_PROCESS);
        imu_pid = start_process(IMU_PROCESS);
        
        if (gnss_pid > 0) {
            printf("Started GNSS process with PID %d\n", gnss_pid);
        }
        if (imu_pid > 0) {
            printf("Started IMU process with PID %d\n", imu_pid);
        }
    } else {
        printf("Recording stopped\n");
        lv_label_set_text(record_label, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(record_btn, lv_color_hex(0xFF0000), LV_PART_MAIN);
        record_control->is_recording = 0;
        is_recording = 0;
        
        // 终止GNSS和IMU进程
        stop_process(gnss_pid);
        stop_process(imu_pid);
        gnss_pid = -1;
        imu_pid = -1;
        printf("Stopped GNSS and IMU processes\n");
    }
}

// Record button event callback (uses trigger function)
static void record_btn_event_cb(lv_event_t *e) {
    // Toggle recording state
    trigger_record_button(!is_recording);
}

// Preview button event callback
static void preview_btn_event_cb(lv_event_t *e) {
    DIR *dir;
    struct dirent *entry;
    char *path = "/mnt/sdcard";
    
    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir failed");
        return;
    }
    
    lv_obj_t *container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(container, 400, 600);
    lv_obj_center(container);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x333333), 0);

    lv_obj_t *list = lv_list_create(container);
    lv_obj_set_size(list, 400, 550);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcmp(ext, ".mkv") == 0 || strcmp(ext, ".json") == 0 || strcmp(ext, ".csv") == 0)) {
                lv_list_add_text(list, entry->d_name);
            }
        }
    }
    closedir(dir);

    lv_obj_t *back_btn = lv_button_create(container);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x444444), 0);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);

    lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);
}

// Menu button event callback
static void menu_btn_event_cb(lv_event_t *e) {
    lv_screen_load(ui_Screen2);
}

// Update video preview
static void update_video_preview(lv_timer_t *timer) {
    lv_obj_t *video_area = (lv_obj_t *)lv_timer_get_user_data(timer);
    static void *shm_ptr = NULL;

    if (!shm_ptr) {
        int shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
        if (shmid == -1) {
            perror("shmget failed");
            return;
        }
        shm_ptr = shmat(shmid, NULL, 0);
        if (shm_ptr == (void *)-1) {
            perror("shmat failed");
            return;
        }
        video_img_dsc.data = shm_ptr;
    }

    sem_wait(sem);
    lv_image_set_src(video_area, &video_img_dsc);
    lv_obj_invalidate(video_area);
    sem_post(sem);
}

// Update GNSS data from FIFO
static void update_gnss_data(lv_timer_t *timer) {
    lv_obj_t *gnss_label = (lv_obj_t *)lv_timer_get_user_data(timer);
    
    int fd = open(GNSS_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        char buffer[32] = {0};
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            int satellites = atoi(buffer);
            
            // 更新卫星数量显示
            lv_label_set_text(gnss_label, buffer);
            
            // 根据卫星数量设置不同颜色
            if (satellites > 5) {
                lv_obj_set_style_text_color(gnss_label, lv_color_hex(0x00FF00), 0); // 绿色
            } else if (satellites > 0) {
                lv_obj_set_style_text_color(gnss_label, lv_color_hex(0xFFFF00), 0); // 黄色
            } else {
                lv_obj_set_style_text_color(gnss_label, lv_color_hex(0xCCCCCC), 0); // 灰色
            }
            
            printf("GNSS update: %s satellites\n", buffer);
        }
    } else if (errno != ENXIO) { // 忽略"没有进程在写入"的错误
        perror("Failed to open GNSS FIFO for reading");
    }
}

// Update recording time
static lv_obj_t *record_time_label;
static void update_record_time(lv_timer_t *timer) {
    if (is_recording) {
        time_t now = time(NULL);
        int elapsed = (int)difftime(now, start_time);
        int hours = elapsed / 3600;
        int minutes = (elapsed % 3600) / 60;
        int seconds = elapsed % 60;
        lv_label_set_text_fmt(record_time_label, "%02d:%02d:%02d", hours, minutes, seconds);
    } else {
        lv_label_set_text(record_time_label, "00:00:00");
    }
}

// Custom SD card display callback
static void custom_sd_display(lv_obj_t *label, uint8_t percent) {
    lv_label_set_text_fmt(label, "%d%%", percent);
    lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    
    // Display red if over 90%
    if (percent > 90) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), 0);
    }
}

void ui_Screen1_screen_init(void) {
    lv_display_t *disp = lv_display_get_default();
    const int32_t hor_res = 480;
    const int32_t ver_res = 800;

    // Initialize semaphore and record control before LVGL setup
    init_semaphore();
    init_record_control();
    init_gnss_fifo();  // 添加GNSS FIFO初始化

    // Create main container
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui_Screen1, hor_res, ver_res);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), 0);

    // Top status bar
    lv_obj_t *status_bar = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, hor_res, 35);
    lv_obj_set_style_pad_hor(status_bar, 15, 0);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // SD card status container
    lv_obj_t *sd_container = lv_obj_create(status_bar);
    lv_obj_remove_style_all(sd_container);
    lv_obj_set_flex_flow(sd_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sd_container, LV_FLEX_ALIGN_CENTER, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // SD card icon
    lv_obj_t *sd_icon = lv_label_create(sd_container);
    lv_label_set_text(sd_icon, LV_SYMBOL_SD_CARD);
    lv_obj_set_style_text_color(sd_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(sd_icon, &lv_font_montserrat_16, 0);
    
    // Initialize SD card status module
    sd_status_init(sd_container);
    sd_set_display_callback(custom_sd_display);

    // Recording mode label
    lv_obj_t *rec_mode = lv_label_create(status_bar);
    lv_label_set_text(rec_mode, "1080P30");
    lv_obj_set_style_text_color(rec_mode, lv_color_white(), 0);
    lv_obj_set_style_text_font(rec_mode, &lv_font_montserrat_16, 0);

    // GNSS satellite info
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

    // Battery info
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

    // Video preview container
    lv_obj_t *video_container = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(video_container);
    lv_obj_set_size(video_container, hor_res, ver_res - 100);
    lv_obj_align(video_container, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(video_container, lv_color_hex(0x222222), 0);

    // Video preview area
    lv_obj_t *video_area = lv_image_create(video_container);
    lv_obj_set_size(video_area, 450, 800);
    lv_obj_center(video_area);
    lv_obj_set_style_bg_color(video_area, lv_color_hex(0x333333), 0);

    // Bottom control bar
    lv_obj_t *bottom_bar = lv_obj_create(ui_Screen1);
    lv_obj_remove_style_all(bottom_bar);
    lv_obj_set_size(bottom_bar, hor_res, 70);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, 
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(bottom_bar, 20, 0);

    // Preview button
    lv_obj_t *preview_btn = lv_button_create(bottom_bar);
    lv_obj_set_size(preview_btn, 50, 50);
    lv_obj_set_style_radius(preview_btn, 25, 0);
    lv_obj_set_style_bg_color(preview_btn, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(preview_btn, preview_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *preview_icon = lv_label_create(preview_btn);
    lv_label_set_text(preview_icon, LV_SYMBOL_VIDEO);
    lv_obj_center(preview_icon);
    lv_obj_set_style_text_color(preview_icon, lv_color_white(), 0);

    // Record button
    record_btn = lv_button_create(ui_Screen1);
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

    // Settings button
    lv_obj_t *menu_btn = lv_button_create(bottom_bar);
    lv_obj_set_size(menu_btn, 50, 50);
    lv_obj_set_style_radius(menu_btn, 25, 0);
    lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x444444), 0);
    lv_obj_add_event_cb(menu_btn, menu_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *menu_icon = lv_label_create(menu_btn);
    lv_label_set_text(menu_icon, LV_SYMBOL_SETTINGS);
    lv_obj_center(menu_icon);
    lv_obj_set_style_text_color(menu_icon, lv_color_white(), 0);

    // Record time label
    record_time_label = lv_label_create(ui_Screen1);
    lv_label_set_text(record_time_label, "00:00:00");
    lv_obj_align(record_time_label, LV_ALIGN_BOTTOM_MID, 0, -100);
    lv_obj_set_style_text_color(record_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(record_time_label, &lv_font_montserrat_24, 0);

    // 创建定时器
    lv_timer_create(update_video_preview, 33, video_area);     // 视频预览更新
    lv_timer_create(update_record_time, 1000, NULL);           // 录制时间更新
    lv_timer_create(update_gnss_data, 1000, gnss_label);       // GNSS数据更新

    lv_screen_load(ui_Screen1);
}