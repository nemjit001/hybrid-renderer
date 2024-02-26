#pragma once

#if		defined(_WIN32)
#define HRI_PLATFORM_WINDOWS	1
#elif	defined(__unix)
#define HRI_PLATFORM_UNIX		1
#endif

#if		HRI_PLATFORM_WINDOWS == 1
#define HRI_ALIGNAS(size)	__declspec(align(size))
#elif	HRI_PLATFORM_UNIX == 1
#define HRI_ALIGNAS(size)	__attribute__((aligned(size)))
#endif

#define HRI_ALIGNED_SIZE(size, align) (unsigned long long)((size + (align - 1)) & ~(align - 1))

#ifndef NDEBUG
#define HRI_DEBUG 1
#else
#define HRI_DEBUG 0
#endif

#if		HRI_PLATFORM_WINDOWS == 1

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#endif
