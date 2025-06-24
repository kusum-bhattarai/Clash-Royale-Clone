cmake_minimum_required(VERSION 3.10)
project(ClashRoyaleClone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE GAME_SOURCES "src/*.cpp")
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/main.cpp$")

add_library(clash_royale_lib ${GAME_SOURCES})

target_include_directories(clash_royale_lib PUBLIC include)

add_executable(run src/core/main.cpp)

target_link_libraries(run PRIVATE clash_royale_lib)

add_executable(
    run_tests 
    test/test_entity_f.cpp
    test/test_entity_behavior.cpp)

target_link_libraries(run_tests PRIVATE clash_royale_lib gtest_main)

include(GoogleTest)
gtest_discover_tests(run_tests)