#pragma once

#define SOEP_ENABLE_PROFILING !RELEASE

#if SOEP_ENABLE_PROFILING 
#include <tracy/Tracy.hpp>
#endif

#if SOEP_ENABLE_PROFILING
#define SOEP_PROFILE_MARK_END			FrameMark;
// NOTE: Use SOEP_PROFILE_FUNC ONLY at the top of a function
//				Use SOEP_PROFILE_SCOPE / SOEP_PROFILE_SCOPE_DYNAMIC for an inner scope
#define SOEP_PROFILE_FUNC(...)			ZoneScoped##__VA_OPT__(N(__VA_ARGS__))
#define SOEP_PROFILE_SCOPE(...)			SOEP_PROFILE_FUNC(__VA_ARGS__)
#define SOEP_PROFILE_SCOPE_DYNAMIC(NAME)  ZoneScoped; ZoneName(NAME, strlen(NAME))
#define SOEP_PROFILE_THREAD(...)          tracy::SetThreadName(__VA_ARGS__)
#else
#define SOEP_PROFILE_MARK_END
#define SOEP_PROFILE_FUNC(...)
#define SOEP_PROFILE_SCOPE(...)
#define SOEP_PROFILE_SCOPE_DYNAMIC(NAME)
#define SOEP_PROFILE_THREAD(...)
#endif