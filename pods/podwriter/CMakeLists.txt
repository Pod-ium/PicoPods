# PicoCalc Writer - distraction-free text editor, on picodev.
# On boards without Wi-Fi, pd_net reports "No Wi-Fi on this board" (F8/F9).
set(_libs pd_diag pd_text pd_epub pd_wifi pd_crosspoint)
picopods_add_pod(picocalc_writer
    SOURCES src/main.cpp src/app.cpp src/app_share.cpp
    LIBS ${_libs}
    TITLE "PicoCalc Writer"
    VERSION "3.0.0")
target_include_directories(picocalc_writer PRIVATE src)
if(NOT PD_HOST)
    target_compile_definitions(picocalc_writer PRIVATE CYW43_HOST_NAME=\"PicoCalcWriter\")
endif()
