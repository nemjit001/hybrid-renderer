#pragma once

#define GLFW_INCLUDE_VULKAN

#include <cassert>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

typedef GLFWwindow WindowHandle;
typedef void (*WindowResizeFunc)(WindowHandle*, int, int);

struct WindowCreateInfo
{
	int32_t width;
	int32_t height;
	const char* pTitle;
	void* pWindowPointer;
	bool resizable;
	WindowResizeFunc pfnResizeFunc;
};

class WindowManager
{
public:
	WindowManager();

	virtual ~WindowManager();

	WindowHandle* createWindow(WindowCreateInfo* createInfo);

	void destroyWindow(WindowHandle* window);

	bool windowShouldClose(WindowHandle* window);

	void pollEvents();

	static VkResult createVulkanSurface(VkInstance instance, WindowHandle* window, VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

private:
	WindowHandle* m_window	= nullptr;
};
