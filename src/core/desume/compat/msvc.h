#pragma once

#if !defined(_MSC_VER)
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#endif
