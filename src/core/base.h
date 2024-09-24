// Define some basic stuff that you want to have defined everywhere

#ifdef PLATFORM_WINDOWS
#define SOEP_FORCE_INLINE __forceinline
#else
#define SOEP_FORCE_INLINE inline
#endif

#include "assert.h"