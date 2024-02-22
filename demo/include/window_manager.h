#pragma once

#define GLFW_INCLUDE_VULKAN

#include <cassert>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

typedef GLFWwindow WindowHandle;
typedef void (*WindowResizeFunc)(WindowHandle*, int, int);

/// @brief The Window Create Info is a structure that lists window creation params.
struct WindowCreateInfo
{
	int32_t width;
	int32_t height;
	const char* pTitle;
	void* pWindowPointer;
	bool resizable;
	WindowResizeFunc pfnResizeFunc;
};

/// @brief The Window Manager handles window state.
class WindowManager
{
public:
	/// @brief Create a new Window Manager instance
	WindowManager();

	/// @brief Destroy this Window Manager
	virtual ~WindowManager();

	/// @brief Create a new Window.
	/// @param createInfo Window Creation info.
	/// @return A new Window handle.
	WindowHandle* createWindow(WindowCreateInfo* createInfo);

	/// @brief Close a window.
	/// @param window Window to close.
	void closeWindow(WindowHandle* window);

	/// @brief Destroy a window.
	/// @param window Window to be destroyed.
	void destroyWindow(WindowHandle* window);

	/// @brief Check if the window should close.
	/// @param window Window to check status for.
	/// @return A boolean indicating closure status.
	bool windowShouldClose(WindowHandle* window) const;

	/// @brief Check if the window is minimized.
	/// @param window Window to check status for.
	/// @return A boolean indicating window minimization.
	bool isWindowMinimized(WindowHandle* window) const;

	/// @brief Poll for window events.
	void pollEvents();

	/// @brief Create a Vulkan render surface.
	/// @param instance Vulkan instance to use.
	/// @param window The Window for which to create a surface.
	/// @param allocator Allocation callbacks.
	/// @param surface The surface handle to populate.
	/// @return A VkResult value indicating success or failure.
	static VkResult createVulkanSurface(VkInstance instance, WindowHandle* window, VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

private:
	WindowHandle* m_window	= nullptr;
};
