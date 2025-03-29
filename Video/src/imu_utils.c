#include "imu_utils.h"
#include <stdio.h>
#include <stdlib.h>

int create_imu_shm(void) {
    int shmid = shmget(IMU_SHM_KEY, IMU_SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed for IMU");
    }
    return shmid;
}

void* attach_imu_shm(int shmid) {
    void *shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat failed for IMU");
    }
    return shm_ptr;
}

void detach_imu_shm(void *shm_ptr) {
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt failed for IMU");
    }
}