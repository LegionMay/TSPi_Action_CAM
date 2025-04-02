cmake_minimum_required(VERSION 3.10)
project(VideoProcess)

# 查找 GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

# 包含头文件目录
include_directories(
    ${GST_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/usr/include  # rkmpp 和 rga 头文件
)

# 设置库文件目录
link_directories(${CMAKE_SOURCE_DIR}/usr/lib)

# 设置源文件
set(SOURCES
    src/main.c
    src/pipeline.c
    src/shm_utils.c
    src/config.c
    src/utils.c
)

# 生成可执行文件到 bin 目录
add_executable(VideoProcess ${SOURCES})
set_target_properties(VideoProcess PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 设置编译选项以支持 GDB 调试
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")

# 链接 GStreamer 和 rkmpp/rga 库
target_link_libraries(VideoProcess
    ${GST_LIBRARIES}
    rockchip_mpp
    rga
    pthread
)