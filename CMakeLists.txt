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
include_directories(libs/tracy/public)
include_directories(libs/pqxx/include)

add_compile_definitions(TRACY_ENABLE) # Needed for tracy to work
add_compile_definitions(TRACY_TIMER_FALLBACK)

if(WIN32)
    message(STATUS "Using local static curl on Windows")
    include_directories(libs/curl/include)
    set(CURL_LIBRARIES
        ${CMAKE_SOURCE_DIR}/libs/curl/lib/libcurl.lib
        ${CMAKE_SOURCE_DIR}/libs/curl/lib/zlib.lib
    )

    file(GLOB PQXX_LIBRARIES "${CMAKE_SOURCE_DIR}/libs/pqxx/lib/*.lib")

    # Static linking requires additional libraries on Windows
    set(WIN_LIBS ws2_32 wldap32 crypt32 advapi32)

    add_definitions(-DCURL_STATICLIB)

else()
    find_package(CURL REQUIRED)
    file(GLOB PQXX_LIBRARIES "${CMAKE_SOURCE_DIR}/libs/pqxx/lib/*.a")
endif()

file(GLOB_RECURSE SRC_FILES src/*.cpp)
set(TRACY_SOURCES libs/tracy/public/TracyClient.cpp)

message(STATUS "Source files: ${SRC_FILES}")

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(DEBUG)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_definitions(RELEASE)
endif()

if(WIN32)
    add_compile_definitions("PLATFORM_WINDOWS=${WIN32}")
else()
    add_compile_definitions("PLATFORM_LINUX=${UNIX}")
endif()

add_executable(SOEP ${SRC_FILES} ${TRACY_SOURCES})

target_precompile_headers(SOEP PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")

# Link libraries based on platform
if(WIN32)
    target_link_libraries(SOEP PRIVATE ${CURL_LIBRARIES} ${WIN_LIBS} ${PQXX_LIBRARIES})
else()
    target_link_libraries(SOEP PRIVATE CURL::libcurl ${PostgreSQL_LIBRARIES} ${PQXX_LIBRARIES})
endif()

# Copy .env file to the output directory
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

# If on Windows, copy additional curl binaries
if(WIN32)
    file(GLOB BIN_FILES "${CMAKE_SOURCE_DIR}/libs/curl/bin/*")

    foreach(BIN_FILE ${BIN_FILES})
        add_custom_command(TARGET SOEP POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BIN_FILE}"
            $<TARGET_FILE_DIR:SOEP>
        )
    endforeach()

    file(GLOB LIBPQXX_FILES "${CMAKE_SOURCE_DIR}/libs/pqxx/bin/*")
     foreach(LIBPQXX_FILE ${LIBPQXX_FILES})
        add_custom_command(TARGET SOEP POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIBPQXX_FILE}"
            $<TARGET_FILE_DIR:SOEP>
        )
    endforeach()
elseif(UNIX)
    if(EXISTS "${CMAKE_SOURCE_DIR}/libs/pqxx/bin/pqxx.a")
    add_custom_command(TARGET SOEP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/libs/pqxx/bin/pqxx.a"
        "$<TARGET_FILE_DIR:SOEP>/pqxx.a"
        COMMENT "Copying pqxx library file to output directory."
    )
    else()
        message(WARNING "No  file detected, the program might crash. Blame Shakir.")
    endif()
endif()



