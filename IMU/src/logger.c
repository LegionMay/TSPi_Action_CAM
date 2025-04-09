/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/logger.c
 */
#include <stdio.h>
#include <sys/msg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include "imu_logger.h"
#define _GNU_SOURCE     // Enable GNU extensions
#include <math.h>       // Math library
#include <stdlib.h>     // Exit and EXIT_FAILURE
#include <unistd.h>     // usleep
#include <sys/stat.h>   // mkdir

// External global variables
extern volatile int g_imu_recording;
extern imu_data_buffer g_buffer;

// Global file variables
static FILE *csv_file = NULL;
static time_t file_start_time = 0;
static char current_filename[256] = {0};
static int record_count = 0; // Count records for periodic logging

// Create message queue
int create_msg_queue(void) {
    int msqid;
    
    // Try to create new queue
    if ((msqid = msgget(MSG_KEY, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        if (errno == EEXIST) { // Queue already exists
            msqid = msgget(MSG_KEY, 0);
            msgctl(msqid, IPC_RMID, NULL); // Delete old queue
            msqid = msgget(MSG_KEY, IPC_CREAT | 0666);
        } else {
            perror("[IMU] msgget failed");
            return -1;
        }
    }
    return msqid;
}

// Send message to queue
void send_to_msg_queue(int msqid, const imu_msg *msg) {
    if (msgsnd(msqid, msg, sizeof(imu_msg) - sizeof(long), 0) == -1) {
        perror("[IMU] msgsnd failed");
    }
}

// Ensure directory exists
static void ensure_directory_exists(const char *path) {
    // Create directory for Linux
    char command[256];
    snprintf(command, sizeof(command), "mkdir -p %s", path);
    system(command);
    printf("[IMU] Ensuring directory exists: %s\n", path);
}

// Create new CSV file
void create_csv_file() {
    // Ensure SD card directory exists
    ensure_directory_exists("/mnt/sdcard");
    printf("[IMU] Attempting to create CSV file...\n");
    
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    
    snprintf(current_filename, sizeof(current_filename), 
           "/mnt/sdcard/imu_%04d%02d%02d_%02d%02d%02d.csv",
           tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec);
    
    printf("[IMU] CSV file path: %s\n", current_filename);
    
    // Multiple attempts to create file
    int retry = 0;
    while (retry < 3) {
        // Close existing file
        if (csv_file) {
            fclose(csv_file);
            csv_file = NULL;
        }
        
        csv_file = fopen(current_filename, "w");
        if (csv_file) {
            fprintf(csv_file, "Timestamp,Roll(deg),Pitch(deg),Yaw(deg)\n");
            // Immediately add initial data line
            fprintf(csv_file, "%ld.000000000,0.00,0.00,0.00\n", time(NULL));
            fflush(csv_file);
            file_start_time = t;
            printf("[IMU] Successfully created CSV file: %s\n", current_filename);
            return;
        } else {
            perror("[IMU] Failed to open file");
            retry++;
            sleep(1);
        }
    }
    
    // If still failing after retries, use /tmp path
    snprintf(current_filename, sizeof(current_filename), 
           "/tmp/imu_%04d%02d%02d_%02d%02d%02d.csv",
           tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec);
    
    csv_file = fopen(current_filename, "w");
    if (csv_file) {
        fprintf(csv_file, "Timestamp,Roll(deg),Pitch(deg),Yaw(deg)\n");
        fprintf(csv_file, "%ld.000000000,0.00,0.00,0.00\n", time(NULL));
        fflush(csv_file);
        file_start_time = t;
        printf("[IMU] Created backup CSV file: %s\n", current_filename);
    } else {
        printf("[IMU] CRITICAL ERROR: Cannot create CSV file anywhere!\n");
    }
}

// Write to CSV file
void write_to_csv(const fused_data *data) {
    if (!csv_file) {
        printf("[IMU] Warning: No CSV file open, attempting to create new file\n");
        create_csv_file();
        if (!csv_file) return;
    }

    // Check if file is older than a day, if so create new file
    time_t t = time(NULL);
    if ((t - file_start_time) >= 86400) {
        printf("[IMU] CSV file is over 24 hours old, creating new file\n");
        create_csv_file();
        if (!csv_file) return;
    }

    // Write data
    fprintf(csv_file, "%ld.%09ld,%.2f,%.2f,%.2f\n", 
          data->ts.tv_sec, data->ts.tv_nsec,
          data->roll * 180/M_PI, 
          data->pitch * 180/M_PI,
          data->yaw * 180/M_PI);
    fflush(csv_file);
    
    // Log periodically to reduce log volume
    if (++record_count % 100 == 0) {
        printf("[IMU] Data recording... (%d records written)\n", record_count);
    }
}

// Close log file
void close_csv_file(void) {
    if (csv_file != NULL) {
        fclose(csv_file);
        csv_file = NULL;
        printf("[IMU] CSV file closed: %s\n", current_filename);
    }
}

// Logging thread
void* logging_thread(void *arg) {
    int msqid = *(int*)arg;
    imu_msg msg;
    int prev_recording_state = 0;
    int empty_data_count = 0;

    printf("[IMU] Logging thread started\n");

    // Create CSV file at startup
    create_csv_file();
    
    // If file creation failed, try an emergency file
    if (!csv_file) {
        printf("[IMU] Attempting to write emergency file...\n");
        csv_file = fopen("/tmp/imu_emergency.csv", "w");
        if (csv_file) {
            fprintf(csv_file, "Timestamp,Roll(deg),Pitch(deg),Yaw(deg)\n");
            fprintf(csv_file, "%ld.000000000,0.00,0.00,0.00\n", time(NULL));
            fflush(csv_file);
            printf("[IMU] Emergency file created successfully\n");
        } else {
            printf("[IMU] CRITICAL: Even emergency file creation failed!\n");
        }
    }
    
    printf("[IMU] Entering logging main loop\n");
    
    while (1) {
        // Detect recording state changes
        if (g_imu_recording && !prev_recording_state) {
            printf("[IMU] Recording state changed: INACTIVE -> ACTIVE\n");
        } else if (!g_imu_recording && prev_recording_state) {
            printf("[IMU] Recording state changed: ACTIVE -> INACTIVE\n");
        }
        prev_recording_state = g_imu_recording;
        
        // Process data
        pthread_mutex_lock(&g_buffer.mutex);
        
        if (g_buffer.count > 0) {
            // Reset empty data counter when we get data
            empty_data_count = 0;
            
            // Get data from buffer
            fused_data data = g_buffer.filtered[g_buffer.head];
            
            // Fill message structure
            msg.mtype = 1; // Message type
            msg.roll = data.roll;
            msg.pitch = data.pitch;
            msg.yaw = data.yaw;
            msg.ts = data.ts;

            // Check if in recording state
            if (g_imu_recording) {
                // Write to file and send message
                write_to_csv(&data);
                send_to_msg_queue(msqid, &msg);
            }

            // Update buffer index
            g_buffer.head = (g_buffer.head + 1) % BUFFER_SIZE;
            g_buffer.count--;
        } else {
            // Periodically log if we're not getting any data
            empty_data_count++;
            if (empty_data_count % 1000 == 0) {
                printf("[IMU] No sensor data received for %d checks\n", empty_data_count);
                
                // Create dummy data if recording is active and we haven't received data
                if (g_imu_recording && csv_file) {
                    time_t now = time(NULL);
                    fprintf(csv_file, "%ld.000000000,0.00,0.00,0.00\n", now);
                    fflush(csv_file);
                    printf("[IMU] Added dummy data point while waiting for sensor\n");
                }
            }
        }
        
        pthread_mutex_unlock(&g_buffer.mutex);
        
        usleep(10000); // 10ms sleep
    }
    return NULL;
}