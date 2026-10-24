# picocalc_ui - shared LCD, text, menu and file-browser code for PicoCalc firmware.
# Usage from a Pico SDK project:
#   add_subdirectory(path/to/picocalc_ui)
#   target_link_libraries(your_app picocalc_ui)
# The browser uses FatFs: your target must have ff.h / ffconf.h on its include path.
add_library(picocalc_ui INTERFACE)
target_sources(picocalc_ui INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/lcd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/gfx.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/ui.cpp
)
target_include_directories(picocalc_ui INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(picocalc_ui INTERFACE pico_stdlib hardware_spi hardware_dma hardware_gpio)
