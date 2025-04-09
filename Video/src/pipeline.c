/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/src/pipeline.c
 */
#include "pipeline.h"
#include "shm_utils.h"
#include "utils.h"
#include "../include/config.h"
#include <gst/app/gstappsink.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

// Shared memory pointer (defined in main.c)
extern void *shm_ptr;

// Semaphore pointer
static sem_t *sem;

// Shared memory for recording control
#define RECORD_SHM_KEY 5678
typedef struct {
    int is_recording;  // 0: not recording, 1: recording
} RecordControl;
static RecordControl *record_control = NULL;

// Pipeline elements (global for access in control functions)
static GstElement *pipeline = NULL;
static GstElement *filesink = NULL;
static GstElement *video_valve = NULL;
static GstElement *audio_valve = NULL;
static gboolean is_recording = FALSE;

// Record command file path
#define RECORD_CMD_FILE "/tmp/record_cmd"

// Thread control
static pthread_t record_thread_id;
static volatile int thread_running = 1;

// Initialize named semaphore
static void init_semaphore(void) {
    sem = sem_open("/preview_sem", O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open failed in video process");
        exit(1);
    }
}

// Initialize recording control shared memory
static void init_record_control(void) {
    int shmid = shmget(RECORD_SHM_KEY, sizeof(RecordControl), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed for record control in video process");
        exit(1);
    }
    record_control = (RecordControl *)shmat(shmid, NULL, 0);
    if (record_control == (void *)-1) {
        perror("shmat failed for record control in video process");
        exit(1);
    }
    record_control->is_recording = 0;  // Initialize to not recording
}

// Appsink callback to write ARGB data to shared memory
static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer user_data) {
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        g_printerr("Failed to pull sample from appsink.\n");
        return GST_FLOW_ERROR;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        g_printerr("Failed to get buffer from sample.\n");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    if (!shm_ptr) {
        g_printerr("Shared memory pointer is NULL.\n");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        // Ensure data size matches (800x450 ARGB = 800 * 450 * 4 = 1,440,000 bytes)
        if (map.size == 1440000) {
            sem_wait(sem);
            memcpy(shm_ptr, map.data, map.size);
            sem_post(sem);
        } else {
            g_printerr("Buffer size mismatch: %lu (expected 1,440,000)\n", map.size);
        }
        gst_buffer_unmap(buffer, &map);
    } else {
        g_printerr("Failed to map buffer.\n");
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// Start recording by opening valves and creating new file
static void start_recording(VideoConfig *config) {
    if (is_recording) {
        g_print("Already recording.\n");
        return;
    }

    g_print("Starting recording...\n");

    // Check if SD card directory exists and is writable
    if (access("/mnt/sdcard", F_OK) != 0) {
        g_print("SD card directory does not exist, creating...\n");
        if (mkdir("/mnt/sdcard", 0755) != 0 && errno != EEXIST) {
            g_printerr("Failed to create SD card directory: %s\n", strerror(errno));
            return;
        }
    } else if (access("/mnt/sdcard", W_OK) != 0) {
        g_printerr("Cannot write to SD card directory: %s\n", strerror(errno));
        return;
    }

    // Ensure pipeline is in the right state for modifying filesink
    GstState current, pending;
    gst_element_get_state(pipeline, &current, &pending, 0);
    
    // First close valves to stop data flow
    g_object_set(G_OBJECT(video_valve), "drop", TRUE, NULL);
    g_object_set(G_OBJECT(audio_valve), "drop", TRUE, NULL);
    
    // Generate filename with timestamp and resolution
    char filename[256];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(filename, sizeof(filename), "/mnt/sdcard/record_%04d%02d%02d_%02d%02d%02d.mkv",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
             t->tm_hour, t->tm_min, t->tm_sec);
    
    // Temporarily set pipeline to READY to safely change filesink location
    gst_element_set_state(filesink, GST_STATE_NULL);
    
    // Set the filesink location to the new file
    g_object_set(G_OBJECT(filesink), "location", filename, NULL);
    g_print("Creating recording file: %s\n", filename);
    
    // Return pipeline to its original state
    gst_element_set_state(filesink, current);
    
    // Open valves to allow data flow and start recording
    g_object_set(G_OBJECT(video_valve), "drop", FALSE, NULL);
    g_object_set(G_OBJECT(audio_valve), "drop", FALSE, NULL);
    
    is_recording = TRUE;
    g_print("Recording started successfully\n");
}

// Stop recording by closing valves
static void stop_recording() {
    if (!is_recording) {
        g_print("Not recording or already stopped.\n");
        return;
    }

    g_print("Stopping recording...\n");
    
    // Close valves to stop data flow
    g_object_set(G_OBJECT(video_valve), "drop", TRUE, NULL);
    g_object_set(G_OBJECT(audio_valve), "drop", TRUE, NULL);
    
    is_recording = FALSE;
    g_print("Recording stopped successfully\n");
}

// Thread to monitor recording control
static void* record_control_thread(void *arg) {
    VideoConfig *config = (VideoConfig *)arg;
    
    g_print("Record control thread started\n");
    
    int prev_state = 0;
    while (thread_running) {
        // Check shared memory control
        if (record_control) {
            int current_state = record_control->is_recording;
            if (current_state != prev_state) {
                g_print("Recording state change (SHM): %d -> %d\n", prev_state, current_state);
                
                if (current_state) {
                    start_recording(config);
                } else {
                    stop_recording();
                }
                prev_state = current_state;
            }
        }
        
        // Check command file
        FILE *cmd_file = fopen(RECORD_CMD_FILE, "r");
        if (cmd_file) {
            char cmd[32] = {0};
            if (fgets(cmd, sizeof(cmd), cmd_file)) {
                // Remove newline if present
                cmd[strcspn(cmd, "\n")] = 0;
                
                g_print("Command received: %s\n", cmd);
                
                if (strcmp(cmd, "start") == 0 && !is_recording) {
                    g_print("Command received to start recording\n");
                    start_recording(config);
                    record_control->is_recording = 1;  // Update shared memory too
                } else if (strcmp(cmd, "stop") == 0 && is_recording) {
                    g_print("Command received to stop recording\n");
                    stop_recording();
                    record_control->is_recording = 0;  // Update shared memory too
                }
            }
            fclose(cmd_file);
            
            // Delete the command file after processing
            remove(RECORD_CMD_FILE);
        }
        
        usleep(100000);  // Check every 100ms
    }
    
    g_print("Record control thread exiting\n");
    return NULL;
}

GstElement* create_pipeline(VideoConfig *config) {
    if (!config) {
        g_printerr("VideoConfig is NULL in create_pipeline.\n");
        return NULL;
    }

    GstElement *source, *tee, *enc_queue, *enc, *parse, *mux,
               *app_queue, *videoscale, *capsfilter_scale, *videoconvert, *capsfilter_rgba, *videoflip, *app_sink,
               *audio_src, *audio_queue, *audio_convert, *audio_resample, *audio_enc;
    GstAppSinkCallbacks callbacks = { NULL, NULL, on_new_sample };

    // Create the pipeline
    pipeline = gst_pipeline_new("video-pipeline");
    
    // Create all elements
    source = gst_element_factory_make("v4l2src", "source");
    tee = gst_element_factory_make("tee", "tee");
    
    // Recording branch elements
    enc_queue = gst_element_factory_make("queue", "enc_queue");
    enc = gst_element_factory_make("mpph265enc", "encoder");
    parse = gst_element_factory_make("h265parse", "parser");
    mux = gst_element_factory_make("matroskamux", "muxer");
    filesink = gst_element_factory_make("filesink", "filesink");
    video_valve = gst_element_factory_make("valve", "video_valve");
    
    // Audio branch elements
    audio_src = gst_element_factory_make("alsasrc", "audio-source");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_convert = gst_element_factory_make("audioconvert", "audio-convert");
    audio_resample = gst_element_factory_make("audioresample", "audio-resample");
    audio_enc = gst_element_factory_make("vorbisenc", "audio-encoder");
    audio_valve = gst_element_factory_make("valve", "audio_valve");
    
    // Preview branch elements
    app_queue = gst_element_factory_make("queue", "app_queue");
    videoscale = gst_element_factory_make("videoscale", "videoscale");
    capsfilter_scale = gst_element_factory_make("capsfilter", "scale_caps");
    videoconvert = gst_element_factory_make("videoconvert", "converter");
    capsfilter_rgba = gst_element_factory_make("capsfilter", "rgba_caps");
    videoflip = gst_element_factory_make("videoflip", "videoflip");
    app_sink = gst_element_factory_make("appsink", "app_sink");

    // Check element creation
    if (!pipeline || !source || !tee || 
        !enc_queue || !enc || !parse || !mux || !filesink || !video_valve ||
        !audio_src || !audio_queue || !audio_convert || !audio_resample || !audio_enc || !audio_valve ||
        !app_queue || !videoscale || !capsfilter_scale || !videoconvert || !capsfilter_rgba || !videoflip || !app_sink) {
        g_printerr("Failed to create pipeline elements\n");
        if (pipeline) gst_object_unref(pipeline);
        return NULL;
    }

    // Configure elements
    // Source configuration
    g_object_set(G_OBJECT(source), "device", "/dev/video0", NULL);
    
    // Queue configuration (to prevent lockups)
    g_object_set(G_OBJECT(enc_queue), 
                "max-size-buffers", 3,
                "max-size-time", 0,
                "leaky", 2, // Downstream leaky queue
                NULL);
    g_object_set(G_OBJECT(app_queue), 
                "max-size-buffers", 3,
                "max-size-time", 0,
                "leaky", 2, // Downstream leaky queue
                NULL);
    g_object_set(G_OBJECT(audio_queue), 
                "max-size-buffers", 3,
                "max-size-time", 0,
                "leaky", 2, // Downstream leaky queue
                NULL);
    
    // Set valves to drop (closed) initially
    g_object_set(G_OBJECT(video_valve), "drop", TRUE, NULL);
    g_object_set(G_OBJECT(audio_valve), "drop", TRUE, NULL);
    
    // Filesink initially points to a dummy file
    g_object_set(G_OBJECT(filesink), "location", "/tmp/dummy.mkv", NULL);
    
    // Audio configuration
    g_object_set(G_OBJECT(audio_src), "device", "hw:0,0", NULL); 
    g_object_set(G_OBJECT(audio_resample), "quality", 2, NULL);
    
    // Preview branch configuration
    g_object_set(G_OBJECT(app_sink), 
                "emit-signals", TRUE,
                "sync", FALSE,
                "drop", TRUE, // Drop buffers if queue full
                "max-buffers", 2,
                NULL);
    gst_app_sink_set_callbacks(GST_APP_SINK(app_sink), &callbacks, NULL, NULL);
    g_object_set(G_OBJECT(videoflip), "method", 1, NULL);  // 90-degree clockwise rotation

    // Set scale caps (NV12 -> 800x450)
    GstCaps *scale_caps = gst_caps_new_simple("video/x-raw",
                                             "format", G_TYPE_STRING, "NV12",
                                             "width", G_TYPE_INT, 800,
                                             "height", G_TYPE_INT, 450,
                                             NULL);
    g_object_set(capsfilter_scale, "caps", scale_caps, NULL);
    gst_caps_unref(scale_caps);

    // Set ARGB conversion caps
    GstCaps *argb_caps = gst_caps_new_simple("video/x-raw",
                                           "format", G_TYPE_STRING, "BGRA",
                                           NULL);
    g_object_set(capsfilter_rgba, "caps", argb_caps, NULL);
    gst_caps_unref(argb_caps);

    // Add all elements to pipeline
    gst_bin_add_many(GST_BIN(pipeline),
                    source, tee, 
                    enc_queue, video_valve, enc, parse, mux, filesink,
                    audio_src, audio_queue, audio_valve, audio_convert, audio_resample, audio_enc,
                    app_queue, videoscale, capsfilter_scale, videoconvert, capsfilter_rgba, videoflip, app_sink,
                    NULL);

    // Set source caps based on config
    GstCaps *src_caps = gst_caps_new_simple("video/x-raw",
                                           "format", G_TYPE_STRING, "NV12",
                                           "width", G_TYPE_INT, config->record_width,
                                           "height", G_TYPE_INT, config->record_height,
                                           "framerate", GST_TYPE_FRACTION, config->record_framerate, 1,
                                           NULL);
    
    // Link source to tee
    if (!gst_element_link_filtered(source, tee, src_caps)) {
        g_printerr("Failed to link source to tee\n");
        gst_caps_unref(src_caps);
        gst_object_unref(pipeline);
        return NULL;
    }
    gst_caps_unref(src_caps);
    
    // Link preview branch
    if (!gst_element_link_many(tee, app_queue, videoscale, capsfilter_scale,
                             videoconvert, capsfilter_rgba, videoflip, app_sink, NULL)) {
        g_printerr("Failed to link preview branch\n");
        gst_object_unref(pipeline);
        return NULL;
    }
    
    // Link recording video branch with valve
    if (!gst_element_link_many(tee, enc_queue, video_valve, enc, parse, mux, filesink, NULL)) {
        g_printerr("Failed to link recording video branch\n");
        gst_object_unref(pipeline);
        return NULL;
    }
    
    // Link recording audio branch with valve
    if (!gst_element_link_many(audio_src, audio_queue, audio_valve, audio_convert, audio_resample, audio_enc, mux, NULL)) {
        g_printerr("Failed to link recording audio branch\n");
        gst_object_unref(pipeline);
        return NULL;
    }
    
    g_print("Pipeline created successfully\n");
    return pipeline;
}

void start_pipeline(GstElement *pipeline_arg, VideoConfig *config) {
    if (!pipeline_arg) {
        g_printerr("Pipeline is NULL.\n");
        return;
    }
    if (!config) {
        g_printerr("VideoConfig is NULL in start_pipeline.\n");
        return;
    }

    // Save pipeline reference globally
    pipeline = pipeline_arg;

    // Initialize semaphore and recording control
    init_semaphore();
    init_record_control();

    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start pipeline\n");
        return;
    }
    
    // Start monitoring thread
    thread_running = 1;
    if (pthread_create(&record_thread_id, NULL, record_control_thread, config) != 0) {
        g_printerr("Failed to create record control thread\n");
    }
    
    g_print("Pipeline started successfully\n");
}

void monitor_pipeline(GstElement *pipeline_arg) {
    if (!pipeline_arg) {
        g_printerr("Pipeline is NULL.\n");
        return;
    }
    
    GstBus *bus = gst_element_get_bus(pipeline_arg);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                                GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    if (msg) {
        GError *err = NULL;
        gchar *debug_info = NULL;
        
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", 
                      GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
        } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            g_print("End of stream reached\n");
        }
        
        gst_message_unref(msg);
    }
    
    gst_object_unref(bus);
}

void cleanup_pipeline(GstElement *pipeline_arg) {
    // Stop recording if active
    if (is_recording) {
        stop_recording();
    }
    
    // Signal thread to exit and wait for it
    thread_running = 0;
    if (record_thread_id) {
        pthread_join(record_thread_id, NULL);
    }
    
    // Clean up pipeline
    if (pipeline_arg) {
        gst_element_set_state(pipeline_arg, GST_STATE_NULL);
        gst_object_unref(pipeline_arg);
    }
    
    // Clean up semaphore
    if (sem) {
        sem_close(sem);
        sem_unlink("/preview_sem");
    }
    
    // Clean up shared memory
    if (record_control) {
        shmdt(record_control);
    }
    
    g_print("Pipeline cleaned up\n");
}