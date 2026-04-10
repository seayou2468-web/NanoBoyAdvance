#pragma once

#if defined(_MSC_VER)
#define RETRO_INLINE __forceinline
#else
#define RETRO_INLINE inline __attribute__((always_inline))
#endif
