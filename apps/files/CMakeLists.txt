picodev_add_app(picocalc_files
    SOURCES src/main.cpp
    LIBS pd_diag pd_usb_drive pd_git pd_wifi pd_bt
    TITLE "PicoCalc Files"
    VERSION "1.2.11")
