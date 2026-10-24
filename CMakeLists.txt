# PicoPods - common PicoCalc libraries and the firmwares (pods) built on them.
#
# Device build (default: PicoCalc + Pico 2 W):
#   cmake -B build -DPICO_SDK_PATH=/path/to/pico-sdk
#   cmake --build build -j
# Other board:    -DPD_BOARD=pico   (pico, pico_w, pico2, pico2_w...)
# PC build (simulator + unit tests):
#   cmake -B build-host -DPD_TERMINAL=host && cmake --build build-host -j
#   ctest --test-dir build-host
cmake_minimum_required(VERSION 3.13)
include(cmake/picopods_init.cmake)
project(picopods C CXX ASM)
include(cmake/picopods.cmake)

add_subdirectory(apps/template)
add_subdirectory(apps/demo)
add_subdirectory(pods/podboy)
add_subdirectory(pods/podwriter)
add_subdirectory(apps/files)
add_subdirectory(apps/usbprobe)
if(PD_HOST)
    enable_testing()
    add_subdirectory(tests)
endif()
