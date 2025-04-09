/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/main.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "imu_logger.h"

// Control FIFO path
#define IMU_FIFO_PATH "/tmp/imu_control_fifo"

// Global control flags
volatile int g_imu_recording = 0;
imu_data_buffer g_buffer = {0};
int g_msqid = -1;
pthread_t g_read_thread, g_log_thread;

// Handle exit signals
static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        printf("[IMU] Process received termination signal, exiting...\n");
        
        // Clean up resources
        close_csv_file();
        
        if (g_msqid != -1) {
            msgctl(g_msqid, IPC_RMID, NULL);
        }
        pthread_mutex_destroy(&g_buffer.mutex);
        
        exit(0);
    }
}

// Command listener thread
void* command_listener_thread(void *arg) {
    int fd;
    char cmd[32];
    
    // Create FIFO
    unlink(IMU_FIFO_PATH);
    if (mkfifo(IMU_FIFO_PATH, 0666) < 0) {
        perror("[IMU] Failed to create IMU control FIFO");
        return NULL;
    }
    
    printf("[IMU] Command listener started. FIFO: %s\n", IMU_FIFO_PATH);
    
    while (1) {
        // Open FIFO to read commands
        fd = open(IMU_FIFO_PATH, O_RDONLY);
        if (fd < 0) {
            perror("[IMU] Failed to open IMU control FIFO");
            sleep(1);
            continue;
        }
        
        // Read command
        memset(cmd, 0, sizeof(cmd));
        read(fd, cmd, sizeof(cmd) - 1);
        close(fd);
        
        // Process command
        if (strcmp(cmd, "start") == 0) {
            g_imu_recording = 1;
            printf("[IMU] Recording started - control flag set to %d\n", g_imu_recording);
        } 
        else if (strcmp(cmd, "stop") == 0) {
            g_imu_recording = 0;
            printf("[IMU] Recording stopped - control flag set to %d\n", g_imu_recording);
        }
        else if (strcmp(cmd, "status") == 0) {
            printf("[IMU] Recording status: %s\n", g_imu_recording ? "active" : "inactive");
        }
    }
    
    return NULL;
}

int main() {
    pthread_t cmd_thread;
    
    // Register signal handlers
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    
    printf("[IMU] Process started\n");
    
    // Ensure directory exists
    system("mkdir -p /mnt/sdcard");
    
    // Immediately try to create a test file to verify filesystem access
    FILE *test_file = fopen("/mnt/sdcard/imu_test.txt", "w");
    if (test_file) {
        fprintf(test_file, "IMU process started at %ld\n", time(NULL));
        fclose(test_file);
        printf("[IMU] Test file created successfully\n");
    } else {
        perror("[IMU] Failed to create test file");
        // Try creating in /tmp
        test_file = fopen("/tmp/imu_test.txt", "w");
        if (test_file) {
            fprintf(test_file, "IMU process started at %ld\n", time(NULL));
            fclose(test_file);
            printf("[IMU] Test file created successfully (in /tmp)\n");
        }
    }
    
    // Immediately create CSV file regardless of sensor status
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char filename[256];
    
    snprintf(filename, sizeof(filename), 
           "/mnt/sdcard/imu_%04d%02d%02d_%02d%02d%02d.csv",
           tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec);
    
    FILE *csv_file = fopen(filename, "w");
    if (!csv_file) {
        perror("[IMU] Failed to create CSV file");
        // Try creating in /tmp
        snprintf(filename, sizeof(filename), 
               "/tmp/imu_%04d%02d%02d_%02d%02d%02d.csv",
               tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
               tm->tm_hour, tm->tm_min, tm->tm_sec);
        
        csv_file = fopen(filename, "w");
        if (!csv_file) {
            perror("[IMU] Failed to create backup CSV file");
        } else {
            fprintf(csv_file, "Timestamp,Roll(deg),Pitch(deg),Yaw(deg)\n");
            fprintf(csv_file, "%ld.000000000,0.00,0.00,0.00\n", time(NULL));
            fclose(csv_file);
            printf("[IMU] Backup CSV file created: %s\n", filename);
        }
    } else {
        fprintf(csv_file, "Timestamp,Roll(deg),Pitch(deg),Yaw(deg)\n");
        fprintf(csv_file, "%ld.000000000,0.00,0.00,0.00\n", time(NULL));
        fclose(csv_file);
        printf("[IMU] CSV file created: %s\n", filename);
    }
    
    // Initialize sensors
    printf("[IMU] Initializing sensors...\n");
    int sensor_ok = 1;
    if (!mpu6500_init("/dev/i2c-1", 0x68)) {
        fprintf(stderr, "[IMU] MPU6500 initialization failed\n");
        sensor_ok = 0;
    } else {
        printf("[IMU] MPU6500 initialized successfully\n");
    }
    
    if (!ak8963_init("/dev/i2c-1", 0x0D)) {
        fprintf(stderr, "[IMU] AK8963 initialization failed\n");
        sensor_ok = 0;
    } else {
        printf("[IMU] AK8963 initialized successfully\n");
    }
    
    if (!sensor_ok) {
        fprintf(stderr, "[IMU] WARNING: Continuing with failed sensor initialization\n");
        // Don't exit - still create files even if sensors are not available
    }

    // Create message queue
    if ((g_msqid = create_msg_queue()) == -1) {
        fprintf(stderr, "[IMU] Failed to create message queue, continuing anyway\n");
    } else {
        printf("[IMU] Message queue created\n");
    }

    // Initialize mutex
    if (pthread_mutex_init(&g_buffer.mutex, NULL) != 0) {
        perror("[IMU] Mutex initialization failed");
        // Continue anyway
    } else {
        printf("[IMU] Mutex initialized\n");
    }

    // Create data collection thread
    printf("[IMU] Starting sensor read thread\n");
    if (pthread_create(&g_read_thread, NULL, sensor_read_thread, &g_buffer) != 0) {
        perror("[IMU] Sensor read thread creation failed");
        // Continue anyway
    } else {
        printf("[IMU] Sensor read thread started\n");
    }

    // Create logging thread - this thread will create CSV file at startup
    printf("[IMU] Starting logging thread\n");
    if (pthread_create(&g_log_thread, NULL, logging_thread, &g_msqid) != 0) {
        perror("[IMU] Logging thread creation failed");
        // Continue anyway
    } else {
        printf("[IMU] Logging thread started\n");
    }
    
    // Wait a second to ensure logging thread has created file
    sleep(1);
    
    // Create command listener thread
    printf("[IMU] Starting command listener thread\n");
    if (pthread_create(&cmd_thread, NULL, command_listener_thread, NULL) != 0) {
        perror("[IMU] Command listener thread creation failed");
        // Continue anyway
    } else {
        printf("[IMU] Command listener thread started\n");
    }

    // Wait for command thread to end (it never will unless terminated by signal)
    printf("[IMU] Main thread waiting for command thread\n");
    pthread_join(cmd_thread, NULL);
    
    // This code should never execute
    close_csv_file();
    msgctl(g_msqid, IPC_RMID, NULL);
    pthread_mutex_destroy(&g_buffer.mutex);
    
    printf("[IMU] Process exited normally\n");
    return 0;
}