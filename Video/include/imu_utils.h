/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/include/imu_utils.h
 */
#ifndef IMU_UTILS_H
#define IMU_UTILS_H

#include <sys/ipc.h>
#include <sys/shm.h>

#define IMU_SHM_KEY 5678
#define IMU_SHM_SIZE 1024  // 根据 IMU 数据大小调整

int create_imu_shm(void);
void* attach_imu_shm(int shmid);
void detach_imu_shm(void *shm_ptr);

#endif // IMU_UTILS_H