#pragma once

#include <spdlog/spdlog.h>

#ifdef PLATFORM_WINDOWS
#define SOEP_DEBUG_BREAK __debugbreak()
#elif defined(PLATFORM_LINUX)
#define SOEP_DEBUG_BREAK __builtin_debugtrap()
#else
#define SOEP_DEBUG_BREAK
#endif


#ifdef DEBUG
#define SOEP_ENABLE_ASSERTS
#endif


#ifdef SOEP_ENABLE_ASSERTS
#ifdef PLATFORM_LINUX
#define SOEP_ASSERT_MESSAGE_INTERNAL(...)  spdlog::error( "Assertion Failed", ##__VA_ARGS__)
#else
#define SOEP_ASSERT_MESSAGE_INTERNAL(...)  spdlog::error("Assertion Failed" __VA_OPT__(,) __VA_ARGS__)
#endif
#define SOEP_ASSERT(condition, ...) { if(!(condition)) { SOEP_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); SOEP_DEBUG_BREAK; } }
#else
#define SOEP_ASSERT(condition, ...)
#endif