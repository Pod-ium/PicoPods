#!/bin/sh
# Builds the host-side app simulator (renders frames to out/*.ppm).
cd "$(dirname "$0")/../.." && g++ -std=c++17 -O1 -g -w -Itest/appsim -Isrc -Ilib/picocalc_ui/include \
  -o test/appsim/appsim \
  test/appsim/main_sim.cpp test/appsim/fakes.cpp test/storage_host.cpp src/hw/lcd.cpp \
  src/ui/app.cpp src/ui/gfx.cpp src/ui/dialogs.cpp src/editor/text_buffer.cpp \
  src/editor/layout.cpp src/editor/syntax.cpp src/ui/app_share.cpp src/export/epub.cpp \
  src/net/x3.cpp test/net_posix.cpp \
  lib/picocalc_ui/src/lcd.cpp lib/picocalc_ui/src/gfx.cpp lib/picocalc_ui/src/ui.cpp
