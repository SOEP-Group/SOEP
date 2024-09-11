cmake_minimum_required(VERSION 3.10)
project(SOEP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_executable(SOEP ${SRC_FILES})

target_compile_options(SOEP PRIVATE -Wall)