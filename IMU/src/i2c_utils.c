/* ---------- src/i2c_utils.c ---------- */
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "imu_logger.h"

// 初始化I2C设备
// Initialize I2C device
int i2c_init(const char *device, int addr) {
    printf("[IMU-I2C] Opening device: %s\n", device);
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("[IMU-I2C] Failed to open I2C device");
        return -1;
    }
    
    printf("[IMU-I2C] Setting I2C slave address: 0x%02X\n", addr);
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        perror("[IMU-I2C] Failed to set I2C slave address");
        close(fd);
        return -1;
    }
    printf("[IMU-I2C] I2C device initialized successfully\n");
    return fd;
}

// Read I2C register
int i2c_read_reg(int fd, uint8_t reg, uint8_t *buf, size_t len) {
    if (write(fd, &reg, 1) != 1) {
        static int error_count = 0;
        if ((++error_count % 1000) == 0) {
            perror("[IMU-I2C] Failed to write register address");
        }
        return -1;
    }
    int ret = read(fd, buf, len);
    if (ret != len) {
        static int error_count = 0;
        if ((++error_count % 1000) == 0) {
            printf("[IMU-I2C] Failed to read from register 0x%02X (read %d bytes, expected %zu)\n", 
                  reg, ret, len);
        }
    }
    return ret;
}

// Write I2C register
int i2c_write_reg(int fd, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    if (write(fd, data, sizeof(data)) != sizeof(data)) {
        perror("[IMU-I2C] Failed to write register");
        return -1;
    }
    return 0;
}