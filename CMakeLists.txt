cmake_minimum_required(VERSION 3.16)

project(ClashRoyale
        VERSION 0.2.0
        DESCRIPTION "A library for building Clash Royale-style real-time strategy games"
        LANGUAGES CXX)

# PROJECT_IS_TOP_LEVEL is CMake 3.21+; derive it so 3.16 consumers still work.
if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL ON)
    else()
        set(PROJECT_IS_TOP_LEVEL OFF)
    endif()
endif()

option(CR_BUILD_TUI      "Build the terminal front-end library and playable game" ${PROJECT_IS_TOP_LEVEL})
option(CR_BUILD_TESTING  "Build the test suite (downloads GoogleTest)"            ${PROJECT_IS_TOP_LEVEL})
option(CR_INSTALL        "Generate install and package-config rules"              ${PROJECT_IS_TOP_LEVEL})
option(CR_BUILD_BENCHMARKS "Build the pathfinding benchmark"                      OFF)

include(GNUInstallDirs)

set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings are applied PRIVATE throughout: they are our problem to fix, not
# something to inflict on projects that consume this library.
if(MSVC)
    set(CR_WARNINGS /W4)
else()
    set(CR_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# cr_core -- the simulation.
#
# Deliberately free of all I/O and platform dependencies, so it builds anywhere
# and can be driven headlessly by bots, tests and training harnesses.
# ---------------------------------------------------------------------------
add_library(cr_core
    src/core/arena.cpp
    src/sim/entity.cpp
    src/sim/board.cpp
    src/sim/combat.cpp
    src/sim/card_registry.cpp
    src/sim/simulation.cpp
    src/path/astar.cpp
    src/path/flow_field.cpp
    src/path/direct.cpp
    src/ai/random_controller.cpp)
add_library(ClashRoyale::core ALIAS cr_core)

target_include_directories(cr_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(cr_core PUBLIC cxx_std_20)
target_compile_options(cr_core PRIVATE ${CR_WARNINGS})
set_target_properties(cr_core PROPERTIES EXPORT_NAME core OUTPUT_NAME clash_royale_core)

# ---------------------------------------------------------------------------
# cr_tui -- the POSIX terminal front-end.
#
# Optional, and off by default when this project is consumed as a subdirectory,
# so downstream users do not compile terminal code they cannot run.
# ---------------------------------------------------------------------------
if(CR_BUILD_TUI)
    add_library(cr_tui
        src/tui/renderer.cpp
        src/tui/input_handler.cpp
        src/tui/game.cpp)
    add_library(ClashRoyale::tui ALIAS cr_tui)

    target_include_directories(cr_tui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(cr_tui PUBLIC cr_core)
    target_compile_options(cr_tui PRIVATE ${CR_WARNINGS})
    set_target_properties(cr_tui PROPERTIES EXPORT_NAME tui OUTPUT_NAME clash_royale_tui)

    add_executable(clash_royale apps/tui/main.cpp)
    target_link_libraries(clash_royale PRIVATE cr_tui)
    target_compile_options(clash_royale PRIVATE ${CR_WARNINGS})
endif()

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
if(CR_BUILD_BENCHMARKS)
    add_executable(path_benchmark benchmarks/path_benchmark.cpp)
    target_link_libraries(path_benchmark PRIVATE cr_core)
    target_compile_options(path_benchmark PRIVATE ${CR_WARNINGS})
endif()

# ---------------------------------------------------------------------------
# Tests
#
# Guarded so that consuming this project via FetchContent or add_subdirectory
# does not force a GoogleTest download into someone else's build.
# ---------------------------------------------------------------------------
if(CR_BUILD_TESTING)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(run_tests
        tests/test_entity_f.cpp
        tests/test_entity_behavior.cpp
        tests/test_simulation.cpp
        tests/test_characterization.cpp
        tests/test_card_registry.cpp
        tests/test_pathfinding.cpp)
    # The suite links cr_core only: nothing under tests/ touches the front-end.
    target_link_libraries(run_tests PRIVATE gtest_main cr_core)
    target_include_directories(run_tests PRIVATE tests)
    target_compile_options(run_tests PRIVATE ${CR_WARNINGS})

    include(GoogleTest)
    gtest_discover_tests(run_tests)
endif()

# ---------------------------------------------------------------------------
# Install / package config
# ---------------------------------------------------------------------------
if(CR_INSTALL)
    include(CMakePackageConfigHelpers)

    set(CR_EXPORT_TARGETS cr_core)
    if(CR_BUILD_TUI)
        list(APPEND CR_EXPORT_TARGETS cr_tui)
    endif()

    install(TARGETS ${CR_EXPORT_TARGETS}
            EXPORT ClashRoyaleTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Install everything except the front-end headers, by exclusion rather than
    # by listing directories -- an earlier version enumerated core/ and sim/ and
    # silently stopped shipping ai/ when that directory was added.
    install(DIRECTORY include/clash_royale
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            REGEX "/tui/" EXCLUDE)
    if(CR_BUILD_TUI)
        install(DIRECTORY include/clash_royale/tui
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clash_royale)
    endif()

    install(EXPORT ClashRoyaleTargets
            FILE ClashRoyaleTargets.cmake
            NAMESPACE ClashRoyale::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ClashRoyale)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ClashRoyaleConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ClashRoyaleConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ClashRoyale)

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/ClashRoyaleConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/ClashRoyaleConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/ClashRoyaleConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ClashRoyale)
endif()
