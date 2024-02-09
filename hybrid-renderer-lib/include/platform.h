#pragma once

#if		defined(_WIN32)
#define HRI_PLATFORM_WINDOWS	1
#elif	defined(__unix)
#define HRI_PLATFORM_UNIX		1
#endif

#if		HRI_PLATFORM_WINDOWS == 1
#define ALIGNAS(size)	__declspec(align(size))
#elif	HRI_PLATFORM_UNIX == 1
#define ALIGNAS(size)	__attribute__((aligned(size)))
#endif
