cmake_minimum_required(VERSION 3.20)

project(dnsrelay C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(DNSRELAY_BUILD_TESTS "Build unit tests" ON)
option(DNSRELAY_BUILD_PERF_TESTS "Register optional performance tests" OFF)
set(DNSRELAY_DEV_PORT "53" CACHE STRING "Development bind port")

add_library(dnsrelay_core
    src/cache.c
    src/config.c
    src/dns_packet.c
    src/local_table.c
    src/logger.c
    src/pending_map.c
    src/platform_posix.c
    src/platform_win.c
    src/server.c
)

target_include_directories(dnsrelay_core PUBLIC include)
target_compile_definitions(dnsrelay_core PUBLIC DNSRELAY_DEV_PORT=${DNSRELAY_DEV_PORT})

if(WIN32)
    target_compile_definitions(dnsrelay_core PRIVATE _WIN32_WINNT=0x0600)
    target_link_libraries(dnsrelay_core PUBLIC ws2_32)
else()
    target_compile_definitions(dnsrelay_core PRIVATE _POSIX_C_SOURCE=200809L)
endif()

target_compile_options(dnsrelay_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(dnsrelay src/main.c)
target_link_libraries(dnsrelay PRIVATE dnsrelay_core)

if(DNSRELAY_BUILD_TESTS)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter)

    add_executable(test_config tests/test_config.c)
    target_link_libraries(test_config PRIVATE dnsrelay_core)

    add_executable(test_local_table tests/test_local_table.c)
    target_link_libraries(test_local_table PRIVATE dnsrelay_core)

    add_executable(test_dns_packet tests/test_dns_packet.c)
    target_link_libraries(test_dns_packet PRIVATE dnsrelay_core)

    add_executable(test_pending_map tests/test_pending_map.c)
    target_link_libraries(test_pending_map PRIVATE dnsrelay_core)

    add_executable(test_cache tests/test_cache.c)
    target_link_libraries(test_cache PRIVATE dnsrelay_core)

    if(NOT WIN32)
        target_compile_definitions(test_config PRIVATE _POSIX_C_SOURCE=200809L)
    endif()

    add_test(NAME test_config COMMAND test_config)
    set_tests_properties(test_config PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_test(NAME test_local_table COMMAND test_local_table)
    set_tests_properties(test_local_table PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_test(NAME test_dns_packet COMMAND test_dns_packet)
    set_tests_properties(test_dns_packet PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_test(NAME test_pending_map COMMAND test_pending_map)
    set_tests_properties(test_pending_map PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_test(NAME test_cache COMMAND test_cache)
    set_tests_properties(test_cache PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    if(Python3_Interpreter_FOUND)
        add_test(
            NAME test_server_integration
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_server_integration.py $<TARGET_FILE:dnsrelay> ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(test_server_integration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

        if(DNSRELAY_BUILD_PERF_TESTS)
            add_test(
                NAME perf_dnsrelay
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/perf_dnsrelay.py $<TARGET_FILE:dnsrelay> ${CMAKE_SOURCE_DIR} --requests 300 --relay-requests 120 --clients 4
            )
            set_tests_properties(perf_dnsrelay PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} LABELS perf)
        endif()
    endif()
endif()
