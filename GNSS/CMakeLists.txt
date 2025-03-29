cmake_minimum_required(VERSION 3.10)
project(GnssCollector VERSION 1.0)

# 设置可执行文件输出目录为bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# 启用调试信息
set(CMAKE_BUILD_TYPE Debug)

# 源文件
set(COLLECTOR_SOURCES
    src/main.c
    src/gnss_reader.c
)

set(CONTROL_SOURCES
    src/gnss_control.c
)

# 可执行文件
add_executable(gnss_collector ${COLLECTOR_SOURCES})
add_executable(gnss_control ${CONTROL_SOURCES})

# 包含目录
target_include_directories(gnss_collector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(gnss_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 链接线程库
find_package(Threads REQUIRED)
target_link_libraries(gnss_collector PRIVATE Threads::Threads)

# 安装目标(可选)
install(TARGETS gnss_collector gnss_control
        RUNTIME DESTINATION bin)