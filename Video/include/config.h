/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/include/config.h
 */
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int record_width ;      // 录制宽度
    int record_height;     // 录制高度
    int record_framerate;  // 录制帧率
    int preview_width;     // 预览宽度
    int preview_height;    // 预览高度
} VideoConfig;

VideoConfig* load_config(void);
void calculate_preview_size(VideoConfig *config, int screen_width, int screen_height);
void free_config(VideoConfig *config);

#endif // CONFIG_H