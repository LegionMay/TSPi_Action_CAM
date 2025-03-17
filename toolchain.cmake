set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm64)

set(tools "/home/lmay/Desktop/TSP_SDK/Release/prebuilts/gcc/linux-x86/aarch64/gcc-buildroot-9.3.0-2020.03-x86_64_aarch64-rockchip-linux-gnu")
set(CMAKE_C_COMPILER ${tools}/bin/aarch64-rockchip-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/aarch64-rockchip-linux-gnu-g++)

set(CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath,${TOOLCHAIN_DIR}/lib")
