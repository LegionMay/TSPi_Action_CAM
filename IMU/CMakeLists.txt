
cmake_minimum_required(VERSION 3.10)
project(IMU_Logger C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra")

add_executable(imu_logger
    src/main.c
    src/i2c_utils.c
    src/mpu6500.c
    src/ak8963.c
    src/sensor_read.c
    src/logger.c)

target_link_libraries(imu_logger pthread)