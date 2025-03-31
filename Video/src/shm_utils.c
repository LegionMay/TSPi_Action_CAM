/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/Video/src/shm_utils.c
 */
#include "shm_utils.h"
#include <stdio.h>
#include <stdlib.h>

int create_shm(void) {
    int shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        return -1;
    }
    return shmid;
}

void* attach_shm(int shmid) {
    void *shm_ptr = shmat(shmid, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat failed");
        return NULL;
    }
    return shm_ptr;
}

void detach_shm(void *shm_ptr) {
    if (shm_ptr && shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
    }
}

void destroy_shm(int shmid) {
    if (shmid != -1 && shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl failed");
    }
}