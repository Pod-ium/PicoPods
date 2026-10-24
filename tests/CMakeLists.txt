# Host tests (cmake -DPD_TERMINAL=host). Run with ctest.
find_package(Python3 COMPONENTS Interpreter)
set(_tmp ${CMAKE_CURRENT_BINARY_DIR}/tmp)
file(MAKE_DIRECTORY ${_tmp})

function(pd_host_test name)
    cmake_parse_arguments(T "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE ${T_LIBS})
    target_compile_options(${name} PRIVATE -g -O1 -fsanitize=address,undefined -Wall)
    target_link_options(${name} PRIVATE -fsanitize=address,undefined)
endfunction()

pd_host_test(core_test SOURCES core_test.cpp LIBS pd_app pd_flash_cache)
add_test(NAME core COMMAND core_test WORKING_DIRECTORY ${_tmp})
set_tests_properties(core PROPERTIES ENVIRONMENT "PD_SDCARD=${_tmp}/sd;PD_KEYS=")

pd_host_test(text_buffer_test SOURCES text_buffer_test.cpp LIBS pd_text)
add_test(NAME text_buffer COMMAND text_buffer_test)
set_tests_properties(text_buffer PROPERTIES TIMEOUT 240)
pd_host_test(reopen_test SOURCES reopen_test.cpp LIBS pd_text)
add_test(NAME reopen COMMAND reopen_test)
pd_host_test(syntax_test SOURCES syntax_test.cpp LIBS pd_text)
add_test(NAME syntax COMMAND syntax_test)

pd_host_test(epub_test SOURCES epub_test.cpp LIBS pd_epub)
set(_samples big.md code.cpp empty.md long.txt nohead.md odd.txt rich.md)
set(_epubs "")
foreach(s ${_samples})
    get_filename_component(stem ${s} NAME_WE)
    add_test(NAME epub_${stem} COMMAND epub_test ${CMAKE_CURRENT_SOURCE_DIR}/data/${s} ${_tmp}/${stem}.epub)
    list(APPEND _epubs ${_tmp}/${stem}.epub)
endforeach()
if(Python3_FOUND)
    add_test(NAME epub_check COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_epub.py ${_epubs})
    set_tests_properties(epub_check PROPERTIES DEPENDS "epub_rich;epub_big;epub_long")

    pd_host_test(crosspoint_test SOURCES crosspoint_test.cpp LIBS pd_crosspoint pd_fs)
    add_test(NAME crosspoint COMMAND ${CMAKE_COMMAND}
        -DPY=${Python3_EXECUTABLE} -DMOCK=${CMAKE_CURRENT_SOURCE_DIR}/mock_crosspoint.py
        -DTEST=$<TARGET_FILE:crosspoint_test> -DEPUB=${_tmp}/rich.epub -DROOT=${_tmp}/x3sd
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_crosspoint.cmake)
    set_tests_properties(crosspoint PROPERTIES DEPENDS epub_rich)
endif()

# The demo app, driven by a key script through every screen, must run to the
# end (frames land in tmp/frames as PPM images, handy to eyeball changes).
file(MAKE_DIRECTORY ${_tmp}/demo_sd)
file(WRITE ${_tmp}/demo_sd/notes.md "# My notes\n\nSome *Markdown* with **bold**.\n\n## Chapter two\n\n- one\n- two\n")
set(_keys "enter,a,shift+b,ctrl+c,esc,esc,down,enter,space,down,enter,wait:400,space,wait:400,esc")
set(_keys "${_keys},down,enter,text: Ettore,enter,text:pw,enter,y,enter,down,enter,enter,esc")
set(_keys "${_keys},down,enter,enter,enter,wait:100,enter,down,enter,enter,wait:100,enter")
set(_keys "${_keys},down,enter,esc,down,enter,enter,down,right,shot")
add_test(NAME demo_script COMMAND pd_demo WORKING_DIRECTORY ${_tmp})
set_tests_properties(demo_script PROPERTIES TIMEOUT 60 ENVIRONMENT
    "PD_SDCARD=${_tmp}/demo_sd;PD_FRAMES=${_tmp}/frames;PD_KEYS=${_keys}")

# PicoCalc Writer: type a document, save it, help, menu, theme, ePub export, Wi-Fi menu.
file(MAKE_DIRECTORY ${_tmp}/writer_sd)
set(_wkeys "wait:200,text:# Hello picodev,enter,enter,text:Some *text* here.,ctrl+s,ctrl+u,text:/hello.md,enter")
set(_wkeys "${_wkeys},f1,esc,f6,f5,esc,shot,esc,f7,enter,wait:200,f9,esc,shot")
add_test(NAME writer_script COMMAND ${CMAKE_COMMAND} -E env
    PD_SDCARD=${_tmp}/writer_sd PD_FRAMES=${_tmp}/writer_frames PD_KEYS=${_wkeys}
    sh -c "rm -rf ${_tmp}/writer_sd/* ${_tmp}/writer_sd/.fwedit && $<TARGET_FILE:picocalc_writer> && test -s ${_tmp}/writer_sd/hello.md && test -s ${_tmp}/writer_sd/hello.epub")
set_tests_properties(writer_script PROPERTIES TIMEOUT 60)

# PicoBoy: starts, shows the ROM browser (no ROMs here), opens a folder.
file(MAKE_DIRECTORY ${_tmp}/picoboy_sd/roms/sub)
add_test(NAME picoboy_script COMMAND picoboy WORKING_DIRECTORY ${_tmp})
set_tests_properties(picoboy_script PROPERTIES TIMEOUT 60 ENVIRONMENT
    "PD_SDCARD=${_tmp}/picoboy_sd;PD_FRAMES=${_tmp}/picoboy_frames;PD_KEYS=down,enter,bksp,shot")
