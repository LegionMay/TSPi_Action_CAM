#ifndef IMU_FILTER_H
#define IMU_FILTER_H

#include "imu_reader.h"

// 滤波器类型
typedef enum {
    FILTER_COMPLEMENTARY, // 互补滤波
    FILTER_MADGWICK,      // Madgwick滤波
    FILTER_EKF            // 扩展卡尔曼滤波
} FilterType;

// 滤波器配置
typedef struct {
    FilterType type;
    float sample_freq;   // 采样频率 (Hz)
    
    // 互补滤波参数
    float comp_alpha;    // 互补滤波系数 (0-1)
    
    // Madgwick参数
    float madgwick_beta; // Madgwick梯度下降增益
    
    // EKF参数
    float ekf_q_angle;   // 角度过程噪声方差
    float ekf_q_bias;    // 角速度偏差过程噪声方差
    float ekf_r_measure; // 测量噪声方差
} FilterConfig;

// 互补滤波器状态
typedef struct {
    float roll;
    float pitch;
    float yaw;
    float q0, q1, q2, q3; // 四元数
    uint64_t last_update;
} ComplementaryFilterState;

// EKF状态
typedef struct {
    float P[2][2]; // 误差协方差矩阵
    float angle;   // 角度
    float bias;    // 陀螺仪偏差
} EKFState;

// 滤波器结构
typedef struct {
    FilterConfig config;
    ComplementaryFilterState comp_state;
    EKFState ekf_roll;
    EKFState ekf_pitch;
    EKFState ekf_yaw;
} ImuFilter;

// 函数声明
void filter_init(ImuFilter* filter, FilterConfig* config);
void filter_update(ImuFilter* filter, ImuRawData* raw_data, ImuProcessedData* processed_data);

// 具体滤波器实现
void complementary_filter_update(ImuFilter* filter, ImuRawData* raw_data, ImuProcessedData* processed_data);
void madgwick_filter_update(ImuFilter* filter, ImuRawData* raw_data, ImuProcessedData* processed_data);
void ekf_filter_update(ImuFilter* filter, ImuRawData* raw_data, ImuProcessedData* processed_data);

#endif