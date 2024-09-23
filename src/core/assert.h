#pragma once

#include <spdlog/spdlog.h>


#ifdef PLATFORM_WINDOWS
#define SOEP_DEBUG_BREAK __debugbreak()
#elif defined(PLATFORM_LINUX)
#include <signal.h>
#define SOEP_DEBUG_BREAK raise(SIGTRAP);
#else
#define SOEP_DEBUG_BREAK
#endif


#ifdef DEBUG
#define SOEP_ENABLE_ASSERTS
#endif

// Use to track bugs during debug. Most stuff should be asserted instead of verified
#ifdef SOEP_ENABLE_ASSERTS
#ifdef PLATFORM_LINUX
#define SOEP_ASSERT_MESSAGE_INTERNAL(...)  spdlog::error("Assertion Failed {}", ##__VA_ARGS__)
#else
#define SOEP_ASSERT_MESSAGE_INTERNAL(...)  spdlog::error("Assertion Failed" __VA_OPT__(,) __VA_ARGS__)
#endif
#define SOEP_ASSERT(condition, ...) { if(!(condition)) { SOEP_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); SOEP_DEBUG_BREAK; } }
#else
#define SOEP_ASSERT(condition, ...)
#endif


// These work similairly to asserts but are still there during release. Use these sparingly on things that you cannot gurantee is fixed
#ifdef PLATFORM_LINUX
#define SOEP_VERIFY_MESSAGE_INTERNAL(...)  spdlog::error( "Verification Failed {}", ##__VA_ARGS__)
#else
#define SOEP_VERIFY_MESSAGE_INTERNAL(...)  spdlog::error("Verification Failed" __VA_OPT__(,) __VA_ARGS__)
#endif
#define SOEP_VERIFY(condition, ...) { if(!(condition)) { SOEP_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); SOEP_DEBUG_BREAK; } }