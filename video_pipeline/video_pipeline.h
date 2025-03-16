#pragma once
#include <stdint.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <gst/gst.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/rk_mpi.h>

#define CAM_WIDTH 1920
#define CAM_HEIGHT 1080

// GStreamer管道
extern GstElement *pipeline;
extern GstElement *source;
extern GstElement *capsfilter;
extern GstElement *queue;
extern GstElement *convert;
extern GstElement *scale;
extern GstElement *videocrop;
extern GstElement *kmssink;
extern GstBus *bus;

// MPP编码
extern MppCtx mpp_ctx;
extern MppApi *mpi;
extern FILE *output_file;
extern gboolean is_recording;

// 视频区域
extern int video_x;
extern int video_y;
extern int video_width;
extern int video_height;

void video_pipeline_init(int x, int y, int width, int height);
void video_pipeline_cleanup(void);
void video_start_record(void);
void video_stop_record(void);
void *video_preview_thread(void *arg);
void *video_record_thread(void *arg);