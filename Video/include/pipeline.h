/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/include/pipeline.h
 */
#ifndef PIPELINE_H
#define PIPELINE_H

#include <gst/gst.h>
#include "config.h"

void* shm_ptr;  // 全局共享内存指针

GstElement* create_pipeline(VideoConfig *config);
void start_pipeline(GstElement *pipeline);
void monitor_pipeline(GstElement *pipeline);
void cleanup_pipeline(GstElement *pipeline);

#endif // PIPELINE_H