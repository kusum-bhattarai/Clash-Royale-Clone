cmake_minimum_required(VERSION 3.10)

project(ClashRoyaleClone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(run ${SOURCES})