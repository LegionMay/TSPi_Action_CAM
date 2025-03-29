#ifndef IPC_COMM_H
#define IPC_COMM_H

#include <stdint.h>
#include "imu_reader.h"

// IPC通信方式
typedef enum {
    IPC_SHARED_MEMORY,
    IPC_MESSAGE_QUEUE
} IPCType;

// 共享内存通信结构
typedef struct {
    int shmid;                  // 共享内存ID
    void* memory;               // 共享内存指针
    int semid;                  // 信号量ID
    char* shmpath;              // 共享内存路径名
} SharedMemoryIPC;

// IPC配置
typedef struct {
    IPCType type;
    char identifier[32];        // 共享内存标识符或消息队列标识符
    int max_data_size;          // 最大数据大小
} IPCConfig;

// IPC通信句柄
typedef struct {
    IPCConfig config;
    SharedMemoryIPC shm;
    int msgid;                  // 消息队列ID
} IPCHandle;

// 函数声明
int ipc_init(IPCHandle* handle, IPCConfig* config);
int ipc_send_data(IPCHandle* handle, ImuProcessedData* data);
int ipc_receive_data(IPCHandle* handle, ImuProcessedData* data);
void ipc_cleanup(IPCHandle* handle);

#endif