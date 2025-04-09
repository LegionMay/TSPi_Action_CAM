/*
 * @Author: LegionMay
 * @FilePath: /TSPi_Action/IMU/src/i2c_utils.h
 */
/* ---------- src/i2c_utils.h ---------- */
#ifndef I2C_UTILS_H
#define I2C_UTILS_H

#include <stdint.h>

// I2C设备初始化
int i2c_init(const char *device, int addr);

// 读取寄存器数据
int i2c_read_reg(int fd, uint8_t reg, uint8_t *buf, size_t len);

// 写入寄存器数据
int i2c_write_reg(int fd, uint8_t reg, uint8_t value);

#endif 