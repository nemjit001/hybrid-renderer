#pragma once

#define GLFW_INCLUDE_VULKAN

#include <cassert>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <hybrid_renderer.h>

#define DEMO_WINDOW_NAME	"Hybrid Rendering Demo"
#define DEMO_APP_NAME       "HybridRenderingDemo"
#define DEMO_APP_VERSION    VK_MAKE_API_VERSION(0, 1, 0, 0)
#define SCR_WIDTH			1280
#define SCR_HEIGHT			720
#define CAMERA_SPEED        1.5f

// Raytracing config
#define DEMO_DEFAULT_RT_RECURSION_DEPTH		1

#ifndef NDEBUG
#define DEMO_DEBUG			1
#define DEMO_DEBUG_OUTPUT	1
#else
#define DEMO_DEBUG			0
#define DEMO_DEBUG_OUTPUT	0
#endif

#define FATAL_ERROR(msg)	(void)(fprintf(stderr, "FATAL ERROR: %s\n", msg), abort(), 0)

// Remove win32 bloat
#ifdef HRI_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#endif
