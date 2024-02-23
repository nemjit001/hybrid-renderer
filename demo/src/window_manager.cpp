#include "window_manager.h"

#define GLFW_INCLUDE_VULKAN

#include <cassert>
#include <cstdint>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

WindowManager::WindowManager()
{
	glfwInit();
}

WindowManager::~WindowManager()
{
	glfwTerminate();
}

WindowHandle* WindowManager::createWindow(WindowCreateInfo* createInfo)
{
	assert(createInfo != nullptr);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, static_cast<int>(createInfo->resizable));
	GLFWwindow* window = glfwCreateWindow(createInfo->width, createInfo->height, createInfo->pTitle, nullptr, nullptr);
	assert(window != nullptr);

	glfwSetWindowUserPointer(window, createInfo->pWindowPointer);
	glfwSetWindowSizeCallback(window, createInfo->pfnResizeFunc);

	m_window = window;
	return window;
}

void WindowManager::closeWindow(WindowHandle* window)
{
	glfwSetWindowShouldClose(window, true);
}

void WindowManager::destroyWindow(WindowHandle* window)
{
	assert(window != nullptr);
	glfwDestroyWindow(window);
	m_window = nullptr;
}

bool WindowManager::windowShouldClose(WindowHandle* window) const
{
	assert(window != nullptr);
	assert(window == m_window);
	return glfwWindowShouldClose(window);
}

bool WindowManager::isWindowMinimized(WindowHandle* window) const
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);

	return width == 0 || height == 0;
}

void WindowManager::pollEvents()
{
	glfwPollEvents();
}

VkResult WindowManager::createVulkanSurface(VkInstance instance, WindowHandle* window, VkAllocationCallbacks* allocator, VkSurfaceKHR* surface)
{
	assert(window != nullptr);
	return glfwCreateWindowSurface(instance, window, allocator, surface);
}
