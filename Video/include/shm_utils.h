#ifndef SHM_UTILS_H
#define SHM_UTILS_H

#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 1234
#define SHM_SIZE 1920 * 1080 * 4  // 默认支持1080p RGBA，可根据需要调整

int create_shm(void);
void* attach_shm(int shmid);
void detach_shm(void *shm_ptr);
void destroy_shm(int shmid);

#endif // SHM_UTILS_H