cmake_minimum_required(VERSION 3.10)
project(ImuProcessor VERSION 1.0)

# 设置可执行文件输出目录为bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# 启用调试信息
set(CMAKE_BUILD_TYPE Debug)

# 源文件
set(IMU_PROCESSOR_SOURCES
    src/main.c
    src/imu_reader.c
    src/imu_filter.c
    src/ipc_comm.c
)

set(CONTROL_SOURCES
    src/imu_control.c
)

# 可执行文件
add_executable(imu_processor ${IMU_PROCESSOR_SOURCES})
add_executable(imu_control ${CONTROL_SOURCES})

# 链接数学库和线程库
target_link_libraries(imu_processor PRIVATE m pthread rt)
target_link_libraries(imu_control PRIVATE rt)

# 安装目标(可选)
install(TARGETS imu_processor imu_control
        RUNTIME DESTINATION bin)