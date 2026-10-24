# picodev - include BEFORE project(): selects the terminal and the board and
# imports the Pico SDK.
#
#   cmake_minimum_required(VERSION 3.13)
#   include(path/to/picodev/cmake/picodev_init.cmake)
#   project(my_app C CXX ASM)
#   include(path/to/picodev/cmake/picodev.cmake)
#   picodev_add_app(my_app SOURCES main.cpp LIBS pd_ui)
#
# Options:
#   -DPD_TERMINAL=picocalc|host   device the firmware runs in (host = PC build)
#   -DPD_BOARD=pico2_w            MCU board: pico, pico_w, pico2, pico2_w, or any
#                                 board name the Pico SDK knows
#   -DPD_PLATFORM=rp2350-arm-s    RP2350 only: rp2350-arm-s (default) or rp2350-riscv
set(PICODEV_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. CACHE INTERNAL "")
set(PD_TERMINAL picocalc CACHE STRING "Terminal: picocalc or host")
set(PD_BOARD pico2_w CACHE STRING "MCU board (Pico SDK board name)")

if(PD_TERMINAL STREQUAL "host")
    set(PD_HOST ON CACHE INTERNAL "")
else()
    set(PD_HOST OFF CACHE INTERNAL "")
    set(PICO_BOARD ${PD_BOARD} CACHE STRING "Board type" FORCE)
    if(PD_BOARD MATCHES "^pico2" AND NOT PICO_PLATFORM)
        set(PD_PLATFORM rp2350-arm-s CACHE STRING "RP2350 platform")
        set(PICO_PLATFORM ${PD_PLATFORM} CACHE STRING "Platform" FORCE)
    endif()
    include(${PICODEV_ROOT}/pico_sdk_import.cmake)
endif()
