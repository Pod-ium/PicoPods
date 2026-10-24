cmake_minimum_required(VERSION 3.13)

# Default to the Pico 2 W (RP2350) that ships in / fits the PicoCalc.
set(PICO_BOARD pico2_w CACHE STRING "Board type")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Platform")

include(pico_sdk_import.cmake)
include(FetchContent)

project(picocalc_writer C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# ---- FatFs R0.15 (ChaN). Fetched from carlk3's repo, which vendors the
# ---- pristine sources; we only use ff.c / ffunicode.c + our own ffconf.h.
set(FATFS_DIR "" CACHE PATH "Optional local FatFs 'source' directory (ff.c, ff.h, ...)")
if(NOT FATFS_DIR)
    FetchContent_Declare(fatfs_src
        GIT_REPOSITORY https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git
        GIT_TAG        d5e453404cdbfaa55ab30d285b6ab0b730e84a05
        GIT_SHALLOW    FALSE)
    FetchContent_GetProperties(fatfs_src)
    if(NOT fatfs_src_POPULATED)
        FetchContent_Populate(fatfs_src)
    endif()
    set(FATFS_DIR ${fatfs_src_SOURCE_DIR}/src/ff15/source)
endif()
# Copy the FatFs sources next to our ffconf.h so ff.h picks up our config.
set(FATFS_GEN ${CMAKE_BINARY_DIR}/fatfs)
file(MAKE_DIRECTORY ${FATFS_GEN})
foreach(f ff.c ff.h ffunicode.c diskio.h)
    configure_file(${FATFS_DIR}/${f} ${FATFS_GEN}/${f} COPYONLY)
endforeach()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/fatfs/ffconf.h ${FATFS_GEN}/ffconf.h COPYONLY)

# Shared PicoCalc UI library (LCD driver, fonts, menus, dialogs), the same
# code PicoBoy uses. Its sources are compiled as part of picocalc_writer.
add_subdirectory(lib/picocalc_ui)

add_executable(picocalc_writer
    src/main.cpp
    src/diag.cpp
    src/hw/lcd.cpp
    src/hw/keyboard.cpp
    src/hw/sdcard.cpp
    src/editor/text_buffer.cpp
    src/editor/layout.cpp
    src/editor/syntax.cpp
    src/editor/storage_fatfs.cpp
    src/ui/gfx.cpp
    src/ui/dialogs.cpp
    src/ui/app.cpp
    src/ui/app_share.cpp
    src/export/epub.cpp
    src/net/x3.cpp
    src/net/net_cyw43.cpp
    ${FATFS_GEN}/ff.c
    ${FATFS_GEN}/ffunicode.c
)
# src/net holds lwipopts.h for the CYW43 lwIP build.
target_include_directories(picocalc_writer PRIVATE ${FATFS_GEN} src src/net)
target_compile_options(picocalc_writer PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -Wall -Wno-psabi>)
target_compile_definitions(picocalc_writer PRIVATE PICO_PANIC_FUNCTION=fw_panic
    CYW43_HOST_NAME=\"PicoCalcWriter\")
target_link_libraries(picocalc_writer
    picocalc_ui
    pico_stdlib hardware_spi hardware_i2c hardware_gpio hardware_watchdog
    pico_cyw43_arch_lwip_poll)

pico_set_program_name(picocalc_writer "PicoCalc Writer")
pico_set_program_version(picocalc_writer "2.1.0")
pico_enable_stdio_uart(picocalc_writer 1)
pico_enable_stdio_usb(picocalc_writer 0)
pico_add_extra_outputs(picocalc_writer)
