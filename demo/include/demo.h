#pragma once

#define GLFW_NO_API
#define GLFW_INCLUDE_VULKAN

#include <cassert>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <hybrid_renderer.h>

#include "platform.h"

#define DEMO_WINDOW_NAME	"Hybrid Rendering Demo"
#define SCR_WIDTH			1280
#define SCR_HEIGHT			720

#define FATAL_ERROR(msg)	(void)(fprintf(stderr, "FATAL ERROR: %s\n", msg), abort(), 0)

// Remove win32 bloat
#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#endif
