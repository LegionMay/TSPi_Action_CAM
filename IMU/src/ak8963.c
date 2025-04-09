/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/ak8963.c
 */
/* ---------- src/ak8963.c ---------- */
#include "imu_logger.h"
#include "i2c_utils.h"

static int ak_fd = -1;

// AK8963初始化
// 改进磁力计初始化

int ak8963_init(const char *device, int addr) {
    printf("[IMU] Initializing AK8963 on %s, address 0x%02X\n", device, addr);
    
    ak_fd = i2c_init(device, addr);
    if (ak_fd < 0) {
        printf("[IMU] Failed to open I2C device for AK8963\n");
        return 0;
    }
    printf("[IMU] I2C device opened successfully for AK8963\n");

    // 重置设备
    if (i2c_write_reg(ak_fd, 0x0A, 0x00) != 0) { // Power-down mode
        printf("[IMU] Failed to reset AK8963\n");
        return 0;
    }
    usleep(100000);  // 100ms delay
    
    // 进入Fuse ROM访问模式以读取校准数据
    if (i2c_write_reg(ak_fd, 0x0A, 0x0F) != 0) { // Fuse ROM access mode
        printf("[IMU] Failed to enter Fuse ROM access mode\n");
        return 0;
    }
    usleep(100000);  // 100ms delay
    
    // 读取校准数据
    uint8_t adjust[3];
    if (i2c_read_reg(ak_fd, 0x10, adjust, 3) != 3) {
        printf("[IMU] Failed to read sensitivity adjustment values\n");
        return 0;
    }
    printf("[IMU] AK8963 adjustment values: %02X %02X %02X\n", 
          adjust[0], adjust[1], adjust[2]);
    
    // 退出Fuse ROM访问模式
    if (i2c_write_reg(ak_fd, 0x0A, 0x00) != 0) {
        printf("[IMU] Failed to exit Fuse ROM access mode\n");
        return 0;
    }
    usleep(100000);  // 100ms delay
    
    // 配置连续测量模式
    if (i2c_write_reg(ak_fd, 0x0A, 0x16) != 0) { // 100Hz with 16-bit resolution
        printf("[IMU] Failed to set continuous measurement mode\n");
        return 0;
    }

    printf("[IMU] AK8963 initialized in continuous measurement mode\n");
    return 1;
}

// Read magnetometer data with debug info
int read_ak8963_data(imu_raw_data *data) {
    uint8_t buf[7];
    
    if (i2c_read_reg(ak_fd, 0x03, buf, sizeof(buf)) != sizeof(buf)) {
        // Only show detailed error every 1000 attempts to avoid log spam
        static int error_count = 0;
        if ((++error_count % 1000) == 0) {
            printf("[IMU] Failed to read from AK8963 (attempt #%d)\n", error_count);
        }
        return 0;
    }

    // Magnetometer data conversion
    data->mag[0] = (int16_t)(buf[1]<<8 | buf[0]) * 0.15; // X axis
    data->mag[1] = (int16_t)(buf[3]<<8 | buf[2]) * 0.15; // Y axis
    data->mag[2] = (int16_t)(buf[5]<<8 | buf[4]) * 0.15; // Z axis
    return 1;
}