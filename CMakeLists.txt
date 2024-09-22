cmake_minimum_required(VERSION 3.16)
project(SOEP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)
include_directories(src)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

include_directories(libs/nlohmann/include)
include_directories(libs/dotenv/include)
include_directories(libs/spdlog/include)

if(WIN32)
    message(STATUS "Using local static curl on Windows")
    include_directories(libs/curl/include)
    set(CURL_LIBRARIES 
        ${CMAKE_SOURCE_DIR}/libs/curl/lib/libcurl.lib 
        ${CMAKE_SOURCE_DIR}/libs/curl/lib/zlib.lib
    )

    # Static linking requires additional libraries on Windows
    set(WIN_LIBS ws2_32 wldap32 crypt32 advapi32)

    add_definitions(-DCURL_STATICLIB)

else()
    find_package(CURL REQUIRED)
endif()

file(GLOB_RECURSE SRC_FILES src/*.cpp)

message(STATUS "Source files: ${SRC_FILES}")

add_compile_definitions("DEBUG=$<CONFIG:Debug>")
add_compile_definitions("RELEASE=$<CONFIG:Release>")
add_compile_definitions("PLATFORM_WINDOWS=${WIN32}")
add_compile_definitions("PLATFORM_LINUX=${UNIX}")
add_executable(SOEP ${SRC_FILES})

target_precompile_headers(SOEP PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")

# Link libraries based on platform
if(WIN32)
     target_link_libraries(SOEP PRIVATE ${CURL_LIBRARIES} ${WIN_LIBS})
else()
     target_link_libraries(SOEP PRIVATE CURL::libcurl)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
    add_custom_command(TARGET SOEP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/.env"
        "$<TARGET_FILE_DIR:SOEP>/.env"
        COMMENT "Copying .env file to output directory."
    )
else()
    message(WARNING "No .env file detected, the program might crash. Ask Shakir for it.")
endif()

if(WIN32)
    file(GLOB BIN_FILES "${CMAKE_SOURCE_DIR}/libs/curl/bin/*")

    foreach(BIN_FILE ${BIN_FILES})
        add_custom_command(TARGET SOEP POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BIN_FILE}"                            
            $<TARGET_FILE_DIR:SOEP>
        )
    endforeach()
endif()
