# PicoCraft - voxel sandbox game on the PicoPods libraries.
# Needs an RP2350 (Pico 2 / Pico 2 W): the world alone is 256 KB of RAM, the
# two frames another 100 KB, more than an RP2040 has.
if(NOT PD_HOST AND PICO_PLATFORM MATCHES "rp2040")
    message(STATUS "PicoCraft skipped: it needs an RP2350 board (pico2, pico2_w)")
    return()
endif()
picopods_add_pod(picocraft
    SOURCES
        src/main.cpp
        src/world.cpp
        src/render.cpp
        src/textures.cpp
        src/gfx.cpp
        src/screen.cpp
        src/storage.cpp
    LIBS pd_diag
    TITLE "PicoCraft"
    VERSION "1.8")
target_include_directories(picocraft PRIVATE src)
target_compile_options(picocraft PRIVATE -O2)
# The ray caster is the bottleneck.
set_source_files_properties(src/render.cpp src/world.cpp PROPERTIES
    COMPILE_OPTIONS "-O3;-ffast-math;-fno-math-errno")
if(NOT PD_HOST)
    # The ray caster runs from RAM (__not_in_flash_func).
    target_compile_definitions(picocraft PRIVATE PICOCRAFT_DEVICE=1)
    # A 2x frame is 320x320 x 18-bit = 300 KB: 98 ms at the terminal's default
    # 25 MHz, 49 ms at 50 MHz (clk_peri 100 MHz / 2), as PodBoy runs it.
    # Set it back to 25000000 if the image glitches.
    target_compile_definitions(picocraft PRIVATE PD_PICOCALC_LCD_HZ=50000000)
endif()
