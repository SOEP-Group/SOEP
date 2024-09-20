cmake_minimum_required(VERSION 3.10)
project(SOEP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)
include_directories(src)

# Find libcurl
find_package(CURL REQUIRED)

# Find nlohmann_json
find_package(nlohmann_json 3.2.0 REQUIRED)

set(SRC_FILES
    src/main.cpp
    src/api/api_call.cpp
)


message(STATUS "Source files: ${SRC_FILES}")

add_executable(SOEP ${SRC_FILES})
# Link libraries
target_link_libraries(SOEP PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
