cmake_minimum_required(VERSION 3.10)
project(lvgl)

# 检查后端配置
foreach(BACKEND_NAME "SDL" "LINUX_DRM" "LINUX_FBDEV" "X11" "WAYLAND")
    execute_process(COMMAND "scripts/backend_conf.sh" ${BACKEND_NAME} OUTPUT_VARIABLE IS_BACKEND_ENABLED)
    set("LV_USE_${BACKEND_NAME}" ${IS_BACKEND_ENABLED})
endforeach()

# 设置 C 和 C++ 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 添加 LVGL 子目录
add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR})

# UI 源文件
set(UI_SOURCES
    ${PROJECT_SOURCE_DIR}/ui/screens/ui_Screen1.c
    ${PROJECT_SOURCE_DIR}/ui/screens/ui_Menu.c
    ${PROJECT_SOURCE_DIR}/ui/sdcard/sd_status.c
    ${PROJECT_SOURCE_DIR}/ui/ui.c
    ${PROJECT_SOURCE_DIR}/ui/components/ui_comp_hook.c
    ${PROJECT_SOURCE_DIR}/ui/ui_helpers.c
)




# 根据不同后端配置编译
if (LV_USE_LINUX_DRM)
    # 查找 libdrm（使用系统默认路径）
    pkg_check_modules(LIBDRM REQUIRED libdrm)

    target_include_directories(lvgl PUBLIC  ${GST_INCLUDE_DIRS} ${GST_APP_INCLUDE_DIRS})
    add_executable(lvglsim main.c mouse_cursor_icon.c ${UI_SOURCES})
    target_include_directories(lvglsim PUBLIC
        ${PROJECT_SOURCE_DIR}/ui
        ${PROJECT_SOURCE_DIR}/ui/screens
        ${PROJECT_SOURCE_DIR}/ui/components
        ${GST_INCLUDE_DIRS}
    )

elseif (LV_USE_SDL)
    # 查找 SDL2 和 SDL2_image（使用系统默认路径）
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

    target_include_directories(lvgl PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${GST_INCLUDE_DIRS} ${GST_APP_INCLUDE_DIRS})
    add_executable(lvglsim main.c mouse_cursor_icon.c ${UI_SOURCES})
    target_include_directories(lvglsim PUBLIC
        ${PROJECT_SOURCE_DIR}/ui
        ${PROJECT_SOURCE_DIR}/ui/screens
        ${PROJECT_SOURCE_DIR}/ui/components
    )
    target_link_libraries(lvglsim lvgl lvgl::examples lvgl::demos lvgl::thorvg ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${GST_LIBRARIES} ${GST_APP_LIBRARIES} m pthread)

elseif (LV_USE_WAYLAND)
    # 查找 Wayland 相关库（使用系统默认路径）
    pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
    pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
    pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
    pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.25)
    pkg_get_variable(WAYLAND_PROTOCOLS_BASE wayland-protocols pkgdatadir)

    execute_process(COMMAND "scripts/gen_wl_protocols.sh" OUTPUT_VARIABLE WAYLAND_PROTOCOLS_SRC)
    target_include_directories(lvgl PRIVATE ${PROJECT_SOURCE_DIR} "${PROJECT_SOURCE_DIR}/wl_protocols" ${GST_INCLUDE_DIRS} ${GST_APP_INCLUDE_DIRS})
    add_executable(lvglsim main.c ${WAYLAND_PROTOCOLS_SRC} mouse_cursor_icon.c backends/wayland.c ${UI_SOURCES})
    target_include_directories(lvglsim PUBLIC
        ${PROJECT_SOURCE_DIR}/ui
        ${PROJECT_SOURCE_DIR}/ui/screens
        ${PROJECT_SOURCE_DIR}/ui/components
    )
    target_compile_definitions(lvglsim PRIVATE LV_CONF_INCLUDE_SIMPLE)
    target_link_libraries(lvglsim lvgl lvgl::examples lvgl::demos lvgl::thorvg ${WAYLAND_CLIENT_LIBRARIES} ${WAYLAND_CURSOR_LIBRARIES} ${XKBCOMMON_LIBRARIES} ${GST_LIBRARIES} ${GST_APP_LIBRARIES} m pthread)

else()
    # 默认情况
    # 确保 LVGL 头文件路径优先
    add_executable(lvglsim main.c mouse_cursor_icon.c ${UI_SOURCES})
    # 包含 /ui/ 路径下的头文件
    target_include_directories(lvglsim PUBLIC
        ${PROJECT_SOURCE_DIR}/ui
        ${PROJECT_SOURCE_DIR}/ui/screens
        ${PROJECT_SOURCE_DIR}/ui/sdcard
        ${PROJECT_SOURCE_DIR}/ui/components
    )
    target_include_directories(lvglsim PUBLIC ${GST_INCLUDE_DIRS})
    target_link_libraries(lvglsim lvgl lvgl::examples lvgl::demos lvgl::thorvg  m pthread)
endif()

# 添加运行目标
add_custom_target(run COMMAND ${EXECUTABLE_OUTPUT_PATH}/lvglsim DEPENDS lvglsim)