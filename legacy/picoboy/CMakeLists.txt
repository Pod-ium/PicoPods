cmake_minimum_required(VERSION 3.13)

# Target: Raspberry Pi Pico 2 W inside a ClockworkPi PicoCalc
set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(picoboy C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(lib/picocalc_ui)

add_executable(picoboy
    src/main.cpp
    src/gb/gb.cpp
    src/gb/cpu.cpp
    src/gb/ppu.cpp
    src/gb/apu.cpp
    src/hw/video.cpp
    src/hw/audio.cpp
    src/hw/keyboard.cpp
    src/hw/sdcard.cpp
    src/hw/romstore.cpp
    lib/fatfs/ff.c
    lib/fatfs/ffunicode.c
)

target_include_directories(picoboy PRIVATE src lib/fatfs)
target_compile_definitions(picoboy PRIVATE
    PICOBOY_DEVICE=1
)
target_compile_options(picoboy PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)

target_link_libraries(picoboy
    picocalc_ui
    pico_stdlib
    pico_multicore
    pico_flash
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pwm
    hardware_irq
    hardware_vreg
    hardware_flash
)

pico_set_program_name(picoboy "PicoBoy")
pico_set_program_description(picoboy "Game Boy emulator for the PicoCalc")
pico_set_program_version(picoboy "1.0")

pico_enable_stdio_uart(picoboy 1)
pico_enable_stdio_usb(picoboy 0)

pico_add_extra_outputs(picoboy)
