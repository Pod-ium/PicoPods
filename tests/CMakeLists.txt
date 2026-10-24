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

pd_host_test(fs_ops_test SOURCES fs_ops_test.cpp LIBS pd_fs)
add_test(NAME fs_ops COMMAND fs_ops_test ${_tmp}/fs_ops_sd)
file(MAKE_DIRECTORY ${_tmp}/fs_ops_sd)

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

# PicoCalc Files: make a folder, copy a file into it, cut/paste, delete, hidden
# toggle, menu, help; the card must end up in the expected state.
set(_fsd ${_tmp}/files_sd)
# (pauses after each operation: the app drops type-ahead once one finishes)
set(_fkeys "wait:100,n,ctrl+u,text:Box,enter,wait:100,down,down,c,up,up,enter,v,wait:100,v,wait:100,shot,down,enter,wait:100")
set(_fkeys "${_fkeys},bksp,down,x,up,enter,v,wait:100,shot,bksp,down,down,d,wait:100,shot,enter,wait:100")
set(_fkeys "${_fkeys},r,ctrl+u,text:Crate,enter,wait:100,esc,esc,wait:50,f1,esc,wait:50,i,wait:50,esc,wait:50")
set(_fkeys "${_fkeys},u,wait:50,esc,wait:50,h,h,s,s,s,shot")
add_test(NAME files_script COMMAND ${CMAKE_COMMAND} -E env
    PD_SDCARD=${_fsd} PD_FRAMES=${_tmp}/files_frames PD_KEYS=${_fkeys}
    sh -c "rm -rf ${_fsd} ${_tmp}/files_frames && mkdir -p ${_fsd}/Docs ${_tmp}/files_frames && echo hello > ${_fsd}/Docs/a.txt && echo world > ${_fsd}/b.txt && echo x > ${_fsd}/c.txt && $<TARGET_FILE:picocalc_files> && sh ${CMAKE_CURRENT_SOURCE_DIR}/check_files.sh ${_fsd}")
set_tests_properties(files_script PROPERTIES TIMEOUT 60)

# Git clone against real servers (needs network; enable with -DPD_NET_TESTS=ON).
if(PD_HAS_TLS)
    pd_host_test(git_clone_test SOURCES git_clone_test.cpp LIBS pd_git)
    if(PD_NET_TESTS)
        add_test(NAME git_clone COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/check_git.sh $<TARGET_FILE:git_clone_test> ${_tmp}/git_sd)
        set_tests_properties(git_clone PROPERTIES TIMEOUT 600)
    endif()
endif()

# Bluetooth OBEX receiver (real OBEX packets from BTstack's builder).
if(PD_HAS_BT)
    pd_host_test(obex_receiver_test SOURCES obex_receiver_test.cpp LIBS pd_bt)
    add_test(NAME obex_receiver COMMAND obex_receiver_test ${_tmp}/obex_sd)
    file(MAKE_DIRECTORY ${_tmp}/obex_sd)
endif()

# PicoCalc Files: receive a file over (simulated) Bluetooth - pairing prompt,
# then a real OBEX Object Push session through the real receiver. The file
# must arrive byte-identical, with its non-ASCII name.
if(PD_HAS_BT)
    set(_bsd ${_tmp}/bt_sd)
    add_test(NAME files_bt_receive COMMAND ${CMAKE_COMMAND} -E env
        PD_SDCARD=${_bsd} PD_FRAMES=${_tmp}/bt_frames "PD_BT_SIM_FILE=${_tmp}/foto città.jpg"
        "PD_KEYS=wait:100,enter,wait:50,b,wait:50,enter,wait:1400,enter,wait:1500,esc,wait:200"
        sh -c "rm -rf ${_bsd} ${_tmp}/bt_frames && mkdir -p ${_bsd}/Inbox ${_tmp}/bt_frames && head -c 40000 /dev/urandom > '${_tmp}/foto città.jpg' && $<TARGET_FILE:picocalc_files> && cmp '${_tmp}/foto città.jpg' '${_bsd}/Inbox/foto città.jpg' && test ! -e '${_bsd}/Inbox/.bt-incoming.part'")
    set_tests_properties(files_bt_receive PROPERTIES TIMEOUT 60)
endif()

# BTstack lifecycle: the real BTstack core (HCI, L2CAP, RFCOMM, SDP, GOEP) on a
# dummy radio, started and stopped 3 times under AddressSanitizer. With the
# Pico SDK's teardown it must fail (use-after-free: the HardFault seen on the
# PicoCalc); with pd_bt's it must run clean.
if(PD_HAS_BT AND DEFINED PICO_SDK_PATH)
    set(_B ${PICO_SDK_PATH}/lib/btstack)
    set(_S ${_B}/src)
    add_executable(btstack_lifecycle_test btstack_lifecycle_test.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/services/bt/src/btstack_modules.cpp
        ${_S}/hci.c ${_S}/hci_cmd.c ${_S}/hci_dump.c ${_S}/l2cap.c ${_S}/l2cap_signaling.c ${_S}/btstack_memory.c
        ${_S}/btstack_memory_pool.c ${_S}/btstack_linked_list.c ${_S}/btstack_util.c ${_S}/btstack_run_loop.c
        ${_S}/btstack_tlv.c ${_S}/ad_parser.c ${_S}/classic/rfcomm.c ${_S}/classic/sdp_server.c ${_S}/classic/sdp_util.c
        ${_S}/classic/sdp_client.c ${_S}/classic/sdp_client_rfcomm.c ${_S}/classic/goep_server.c ${_S}/classic/goep_client.c
        ${_S}/classic/obex_message_builder.c ${_S}/classic/obex_parser.c ${_S}/classic/obex_srm_client.c
        ${_S}/classic/obex_srm_server.c ${_S}/classic/btstack_link_key_db_memory.c ${_S}/classic/gatt_sdp.c
        ${_B}/platform/posix/btstack_run_loop_posix.c)
    target_include_directories(btstack_lifecycle_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/btstack_host ${_S}
        ${_B}/platform/posix ${_B}/3rd-party/bluedroid/encoder/include ${_B}/3rd-party/bluedroid/decoder/include
        ${_B}/3rd-party/micro-ecc ${_B}/3rd-party/rijndael ${_B}/3rd-party/lc3-google/include ${_B}/3rd-party/yxml)
    target_compile_options(btstack_lifecycle_test PRIVATE -fsanitize=address -g -w)
    target_link_options(btstack_lifecycle_test PRIVATE -fsanitize=address)
    add_test(NAME btstack_lifecycle COMMAND btstack_lifecycle_test)
    add_test(NAME btstack_lifecycle_sdk_teardown_crashes COMMAND btstack_lifecycle_test --old)
    set_tests_properties(btstack_lifecycle_sdk_teardown_crashes PROPERTIES WILL_FAIL TRUE)
endif()

# Pairing started by the phone while the device list is open: the request must
# be shown and answered from inside the menu (ui idle hook + key::Repaint).
if(PD_HAS_BT)
    add_test(NAME files_bt_phone_pairing COMMAND ${CMAKE_COMMAND} -E env
        PD_SDCARD=${_tmp}/btp_sd PD_BT_SIM_PHONE_PAIR=1
        "PD_KEYS=wait:100,b,wait:50,enter,wait:3000,enter,wait:200,esc,wait:100"
        sh -c "rm -rf ${_tmp}/btp_sd && mkdir -p ${_tmp}/btp_sd && echo hi > ${_tmp}/btp_sd/doc.txt && $<TARGET_FILE:picocalc_files>")
    set_tests_properties(files_bt_phone_pairing PROPERTIES PASS_REGULAR_EXPRESSION "pairing answered: yes" TIMEOUT 60)
endif()
