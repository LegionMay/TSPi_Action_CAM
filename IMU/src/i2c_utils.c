/* ---------- src/i2c_utils.c ---------- */
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "imu_logger.h"

// 初始化I2C设备
int i2c_init(const char *device, int addr) {
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("I2C设备打开失败");
        return -1;
    }
    
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        perror("I2C地址设置失败");
        close(fd);
        return -1;
    }
    return fd;
}

// 读取I2C寄存器
int i2c_read_reg(int fd, uint8_t reg, uint8_t *buf, size_t len) {
    if (write(fd, &reg, 1) != 1) {
        perror("寄存器写入失败");
        return -1;
    }
    return read(fd, buf, len);
}

// 写入I2C寄存器
int i2c_write_reg(int fd, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    if (write(fd, data, sizeof(data)) != sizeof(data)) {
        perror("寄存器写入失败");
        return -1;
    }
    return 0;
}