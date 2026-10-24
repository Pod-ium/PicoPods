# PicoBoy - Game Boy (DMG) emulator on picodev.
# Needs an RP2350 (Pico 2 / Pico 2 W): the 2x-scaled video buffer alone is
# 276 KB, more than an RP2040 has.
if(NOT PD_HOST AND PICO_PLATFORM MATCHES "rp2040")
    message(STATUS "PicoBoy skipped: it needs an RP2350 board (pico2, pico2_w)")
    return()
endif()
picodev_add_app(picoboy
    SOURCES
        src/main.cpp
        src/gb/gb.cpp
        src/gb/cpu.cpp
        src/gb/ppu.cpp
        src/gb/apu.cpp
    LIBS pd_flash_cache
    TITLE "PicoBoy"
    VERSION "1.2")
target_include_directories(picoboy PRIVATE src)
target_compile_options(picoboy PRIVATE -O2)
if(NOT PD_HOST)
    # Hot emulator functions go to RAM: the XIP cache is left to the ROM data.
    target_compile_definitions(picoboy PRIVATE PICOBOY_DEVICE=1)
    target_link_libraries(picoboy PRIVATE hardware_vreg)
endif()
# Older mainboards route the left audio channel to GP26:
#   target_compile_definitions(picoboy PRIVATE PD_PICOCALC_AUDIO_L=26)
