# pd_terminal_host: headless PC terminal (framebuffer + PPM dumps, scripted keys).
add_library(pd_terminal_host INTERFACE)
target_sources(pd_terminal_host INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/host.cpp)
target_include_directories(pd_terminal_host INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(pd_terminal_host INTERFACE pd_terminal_api pd_fs pd_input pd_core)
