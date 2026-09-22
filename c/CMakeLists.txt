cmake_minimum_required(VERSION 3.20)
project(tdx_l1stream VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(TDX_ENABLE_SANITIZERS "Run tests with address and undefined-behavior checks" OFF)
option(TDX_BUILD_FUZZERS "Build Clang libFuzzer codec target" OFF)
find_package(ZLIB QUIET)
find_package(Threads REQUIRED)
if(NOT WIN32)
    find_package(Iconv REQUIRED)
endif()

add_library(tdx_build_options INTERFACE)
target_include_directories(tdx_build_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(tdx_build_options INTERFACE /W4 /utf-8)
    # The portable C API intentionally uses standard, bounds-checked libc calls.
    target_compile_definitions(tdx_build_options INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(tdx_build_options INTERFACE -Wall -Wextra -Wpedantic)
endif()
if(MINGW)
    target_compile_definitions(tdx_build_options INTERFACE __USE_MINGW_ANSI_STDIO=1)
endif()
if(NOT WIN32)
    target_compile_definitions(tdx_build_options INTERFACE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
endif()
if(TDX_ENABLE_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR WIN32)
        message(FATAL_ERROR "TDX_ENABLE_SANITIZERS requires a POSIX GCC/Clang build")
    endif()
    target_compile_options(tdx_build_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(tdx_build_options INTERFACE -fsanitize=address,undefined)
endif()

# Dependency direction: base <- protocol <- domain <- runtime <- CLI.
# Each library can be linked independently; tdxl1core preserves the aggregate target.
add_library(tdx_base STATIC
    src/tdx_bytes.c
    src/tdx_text.c
    src/tdx_thread.c
    src/tdx_md5.c
    src/tdx_date.c
    src/tdx_cache.c
    src/tdx_json.c
    src/tdx_json_write.c
    src/tdx_zip.c
)
target_link_libraries(tdx_base PUBLIC tdx_build_options Threads::Threads)
target_include_directories(tdx_base PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(tdx_protocol STATIC
    src/tdx_endpoint.c
    src/tdx_frame.c
    src/tdx_quote.c
    src/tdx_directory.c
    src/tdx_download.c
    src/tdx_jsn.c
    src/tdx_jsn_json.c
)
target_link_libraries(tdx_protocol PUBLIC tdx_base)
target_include_directories(tdx_protocol PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(tdx_domain STATIC
    src/tdx_auction.c
    src/tdx_auction_json.c
    src/tdx_blocks.c
    src/tdx_blocks_json.c
    src/tdx_bond_math.c
    src/tdx_bonds.c
    src/tdx_bonds_json.c
    src/tdx_capital.c
    src/tdx_capital_json.c
    src/tdx_convertible.c
    src/tdx_convertible_join.c
    src/tdx_convertible_json.c
    src/tdx_daily.c
    src/tdx_daily_json.c
    src/tdx_directory_json.c
    src/tdx_finance.c
    src/tdx_finance_json.c
    src/tdx_format.c
    src/tdx_gbbq.c
    src/tdx_industry.c
    src/tdx_industry_json.c
    src/tdx_kline.c
    src/tdx_kline_json.c
    src/tdx_limit.c
    src/tdx_limit_json.c
    src/tdx_limits.c
    src/tdx_limits_json.c
    src/tdx_minute.c
    src/tdx_minute_json.c
    src/tdx_newbond.c
    src/tdx_newbond_json.c
    src/tdx_panorama.c
    src/tdx_panorama_json.c
    src/tdx_panorama_views.c
    src/tdx_pending.c
    src/tdx_pending_json.c
    src/tdx_pricing.c
    src/tdx_pricing_json.c
    src/tdx_professional.c
    src/tdx_professional_finance.c
    src/tdx_professional_finance_json.c
    src/tdx_professional_json.c
    src/tdx_ranking.c
    src/tdx_ranking_json.c
    src/tdx_seal.c
    src/tdx_seal_json.c
    src/tdx_snapshot.c
    src/tdx_snapshot_json.c
    src/tdx_state.c
    src/tdx_subscription.c
    src/tdx_subscription_json.c
    src/tdx_timeline.c
    src/tdx_timeline_json.c
    src/tdx_trades.c
    src/tdx_trades_json.c
    src/tdx_valuation.c
    src/tdx_valuation_json.c
    src/tdx_zst.c
    src/tdx_zst_day.c
    src/tdx_zst_json.c
    src/tdx_zst_replay.c
)
target_link_libraries(tdx_domain PUBLIC tdx_protocol)
target_include_directories(tdx_domain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(tdx_runtime STATIC
    src/tdx_pool.c
    src/tdx_hub.c
    src/tdx_serve.c
)
target_link_libraries(tdx_runtime PUBLIC tdx_domain)
target_include_directories(tdx_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(ZLIB_FOUND)
    target_link_libraries(tdx_base PUBLIC ZLIB::ZLIB)
else()
    # The compiler sysroot may provide zlib without CMake package metadata.
    target_link_libraries(tdx_base PUBLIC z)
endif()
if(WIN32)
    target_link_libraries(tdx_protocol PUBLIC ws2_32)
else()
    target_link_libraries(tdx_base PUBLIC m Iconv::Iconv)
endif()
add_library(tdxl1core INTERFACE)
target_link_libraries(tdxl1core INTERFACE tdx_runtime)

add_library(tdx_cli STATIC
    src/cli/cli_date.c
    src/cli/cli_help.c
    src/cli/cli_options.c
    src/cli/cli_registry.c
    src/cli/cli_support.c
    src/cli/command_auction.c
    src/cli/command_blocks.c
    src/cli/command_capital.c
    src/cli/command_convertible.c
    src/cli/command_daily.c
    src/cli/command_day.c
    src/cli/command_finance.c
    src/cli/command_industry.c
    src/cli/command_jsn.c
    src/cli/command_kline.c
    src/cli/command_limit.c
    src/cli/command_limits.c
    src/cli/command_minute.c
    src/cli/command_newbond.c
    src/cli/command_panorama.c
    src/cli/command_pending.c
    src/cli/command_pricing.c
    src/cli/command_probe.c
    src/cli/command_professional.c
    src/cli/command_ranking.c
    src/cli/command_seal.c
    src/cli/command_securities.c
    src/cli/command_serve.c
    src/cli/command_snapshot.c
    src/cli/command_subscription.c
    src/cli/command_sweep.c
    src/cli/command_timeline.c
    src/cli/command_trades.c
    src/cli/command_valuation.c
    src/cli/command_watch.c
 )
target_link_libraries(tdx_cli PUBLIC tdxl1core)
target_include_directories(tdx_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/cli)
add_executable(tdx-l1stream src/main.c)
target_link_libraries(tdx-l1stream PRIVATE tdx_cli)

include(CTest)
if(BUILD_TESTING)
    add_executable(tdx-l1stream-cli-tests tests/test_cli.c)
    target_link_libraries(tdx-l1stream-cli-tests PRIVATE tdx_cli)
    add_test(NAME l1stream-cli COMMAND tdx-l1stream-cli-tests)
    foreach(name
        auction
        blocks
        bond_math
        bonds
        bytes
        cache
        capital
        convertible
        convertible_join
        daily
        date
        directory
        domain_renderers
        download
        endpoint
        finance
        frame
        gbbq
        hub
        hub_lifecycle
        industry
        jsn
        json
        kline
        limit
        limits
        minute
        minute_pack
        newbond
        panorama
        pending
        pool
        pricing
        professional
        professional_finance
        quote
        ranking
        seal
        serve
        snapshot
        state
        subscription
        timeline
        trades
        valuation
        zst
    )
        add_executable(tdx-l1stream-${name}-tests tests/test_${name}.c)
        target_link_libraries(tdx-l1stream-${name}-tests PRIVATE tdxl1core)
        add_test(NAME l1stream-${name} COMMAND tdx-l1stream-${name}-tests)
        set_tests_properties(l1stream-${name} PROPERTIES TIMEOUT 60)
    endforeach()
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME l1stream-json-independent
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_json_output.py
                $<TARGET_FILE:tdx-l1stream-jsn-tests>)
    add_test(NAME l1stream-domain-json-independent
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_domain_renderers.py
                $<TARGET_FILE:tdx-l1stream-domain_renderers-tests>)
    add_test(NAME l1stream-cli-contracts
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_contracts.py
                $<TARGET_FILE:tdx-l1stream>)
    add_test(NAME l1stream-generated
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate.py --check)
    set_tests_properties(l1stream-cli-contracts l1stream-json-independent l1stream-domain-json-independent l1stream-generated
        PROPERTIES TIMEOUT 60)
endif()
if(Python3_Interpreter_FOUND)
    add_custom_target(tdx-verify-generated
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate.py --check
        COMMENT "Verify reproducible C tables and fixtures")
endif()
if(TDX_BUILD_FUZZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR WIN32)
        message(FATAL_ERROR "TDX_BUILD_FUZZERS requires a POSIX Clang build")
    endif()
    target_compile_options(tdx_base PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(tdx_protocol PRIVATE -fsanitize=fuzzer-no-link)
    add_executable(tdx-fuzz-codecs tests/fuzz_codecs.c)
    target_link_libraries(tdx-fuzz-codecs PRIVATE tdx_protocol)
    target_compile_options(tdx-fuzz-codecs PRIVATE -fsanitize=fuzzer)
    target_link_options(tdx-fuzz-codecs PRIVATE -fsanitize=fuzzer)
endif()
