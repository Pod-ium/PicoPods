# pd_tls: TLS client (mbedTLS 2.28) over pd::net::Tcp, plus a small HTTP/1.1
# client for http:// and https:// (pd/tls.h). Device: the Pico SDK's mbedTLS
# with config/mbedtls_config.h and built-in root CAs. Host: the system
# mbedTLS 2.28 (Ubuntu: libmbedtls-dev), trusting the system CA store too.
add_library(pd_tls INTERFACE)
target_include_directories(pd_tls INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
target_sources(pd_tls INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/tls.cpp ${CMAKE_CURRENT_LIST_DIR}/src/http.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/ca_bundle.cpp)
target_link_libraries(pd_tls INTERFACE pd_net pd_core)
if(PD_HOST)
    find_library(PD_MBEDTLS_LIB mbedtls)
    find_library(PD_MBEDX509_LIB mbedx509)
    find_library(PD_MBEDCRYPTO_LIB mbedcrypto)
    if(PD_MBEDTLS_LIB AND PD_MBEDX509_LIB AND PD_MBEDCRYPTO_LIB)
        set(PD_HAS_TLS ON CACHE INTERNAL "")
        target_link_libraries(pd_tls INTERFACE ${PD_MBEDTLS_LIB} ${PD_MBEDX509_LIB} ${PD_MBEDCRYPTO_LIB})
    else()
        set(PD_HAS_TLS OFF CACHE INTERNAL "")
        message(STATUS "picodev: mbedTLS not found (apt install libmbedtls-dev): TLS/Git apps and tests skipped")
    endif()
else()
    set(PD_HAS_TLS ${PD_HAS_WIFI} CACHE INTERNAL "")
    target_include_directories(pd_tls INTERFACE ${CMAKE_CURRENT_LIST_DIR}/config)
    target_link_libraries(pd_tls INTERFACE pico_mbedtls)
endif()
