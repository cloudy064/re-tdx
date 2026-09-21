cmake_minimum_required(VERSION 3.20)
project(tdx_l1stream VERSION 0.1.0 LANGUAGES C)

# Standalone C implementation of the public L1 quote stream.
# It is deliberately independent from ../native (the C++ research tool): the
# two are meant to be diffed against each other on live data.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# zlib is required for 7709 response inflation.  Prefer a discovered package, but
# fall back to the plain library name so an MSYS2/MinGW sysroot that ships
# zlib.h and libz.a works without extra cache variables.
find_package(ZLIB QUIET)

add_library(tdxl1core STATIC
    src/tdx_bytes.c
    src/tdx_text.c
    src/tdx_thread.c
    src/tdx_endpoint.c
    src/tdx_frame.c
    src/tdx_quote.c
    src/tdx_directory.c
    src/tdx_pool.c
    src/tdx_state.c
    src/tdx_format.c
    src/tdx_hub.c
    src/tdx_serve.c
    src/tdx_zst.c
    src/tdx_zst_replay.c
    src/tdx_zst_json.c
    src/tdx_zst_day.c
    src/tdx_md5.c
    src/tdx_download.c
    src/tdx_trades.c
    src/tdx_trades_json.c
    src/tdx_kline.c
    src/tdx_kline_json.c
    src/tdx_timeline.c
    src/tdx_timeline_json.c
    src/tdx_auction.c
    src/tdx_auction_json.c
    src/tdx_snapshot.c
    src/tdx_snapshot_json.c
)
target_include_directories(tdxl1core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(ZLIB_FOUND)
    target_link_libraries(tdxl1core PUBLIC ZLIB::ZLIB)
else()
    target_link_libraries(tdxl1core PUBLIC z)
endif()

if(WIN32)
    target_link_libraries(tdxl1core PUBLIC ws2_32)
endif()

if(MINGW)
    # MinGW links the legacy MSVCRT printf by default, which has no %zu under
    # -std=c11.  Select the project own ANSI implementation instead.
    target_compile_definitions(tdxl1core PUBLIC __USE_MINGW_ANSI_STDIO=1)
else()
    target_link_libraries(tdxl1core PUBLIC m)
endif()

if(MSVC)
    target_compile_options(tdxl1core PRIVATE /W4)
else()
    target_compile_options(tdxl1core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(tdx-l1stream src/main.c)
target_link_libraries(tdx-l1stream PRIVATE tdxl1core)

enable_testing()

foreach(pair "frame:test_frame" "quote:test_quote" "directory:test_directory"
             "pool:test_pool" "state:test_state" "hub:test_hub" "zst:test_zst"
             "download:test_download" "endpoint:test_endpoint" "trades:test_trades"
             "kline:test_kline" "timeline:test_timeline" "auction:test_auction"
             "snapshot:test_snapshot")
    string(REPLACE ":" ";" parts "${pair}")
    list(GET parts 0 name)
    list(GET parts 1 source)
    add_executable(tdx-l1stream-${name}-tests tests/${source}.c)
    target_link_libraries(tdx-l1stream-${name}-tests PRIVATE tdxl1core)
    add_test(NAME l1stream-${name} COMMAND tdx-l1stream-${name}-tests)
endforeach()
