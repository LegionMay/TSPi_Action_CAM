/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/ak8963.c
 */
/* ---------- src/ak8963.c ---------- */
#include "imu_logger.h"
#include "i2c_utils.h"

static int ak_fd = -1;

// AK8963初始化
int ak8963_init(const char *device, int addr) {
    ak_fd = i2c_init(device, addr);
    if (ak_fd < 0) return 0;

    // 初始化磁力计
    uint8_t init_seq[] = {
        0x0A, 0x16,   // 连续测量模式2，100Hz
        0x0B, 0x01    // 自检模式
    };

    for (int i=0; i<sizeof(init_seq); i+=2) {
        if (i2c_write_reg(ak_fd, init_seq[i], init_seq[i+1]) != 0) {
            return 0;
        }
    }
    return 1;
}

// 读取磁力计数据
int read_ak8963_data(imu_raw_data *data) {
    uint8_t buf[7];
    
    if (i2c_read_reg(ak_fd, 0x03, buf, sizeof(buf)) != sizeof(buf))
        return 0;

    // 磁力计数据转换
    data->mag[0] = (int16_t)(buf[1]<<8 | buf[0]) * 0.15; // X轴
    data->mag[1] = (int16_t)(buf[3]<<8 | buf[2]) * 0.15; // Y轴 
    data->mag[2] = (int16_t)(buf[5]<<8 | buf[4]) * 0.15; // Z轴
    return 1;
}