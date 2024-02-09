#include <GLFW/glfw3.h>
#include <iostream>

#include "demo.h"
#include "timer.h"

static Timer gFrameTimer = Timer();
static GLFWwindow* gWindow = nullptr;

struct WindowCreateInfo
{
	int32_t width;
	int32_t height;
	const char* pTitle;
	void* pWindowPointer;
	GLFWwindowsizefun pfnResizeFun;
};

GLFWwindow* initWindow(WindowCreateInfo* createInfo)
{
	assert(createInfo != nullptr);
	glfwInit();

	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	GLFWwindow* window = glfwCreateWindow(createInfo->width, createInfo->height, createInfo->pTitle, nullptr, nullptr);
	assert(window != nullptr);
	
	glfwSetWindowUserPointer(window, createInfo->pWindowPointer);
	glfwSetWindowSizeCallback(window, createInfo->pfnResizeFun);

	return window;
}

void destroyWindow(GLFWwindow* window)
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

int main()
{
	WindowCreateInfo windowCreateInfo = WindowCreateInfo{};
	windowCreateInfo.width = SCR_WIDTH;
	windowCreateInfo.height = SCR_HEIGHT;
	windowCreateInfo.pTitle = DEMO_WINDOW_NAME;
	gWindow = initWindow(&windowCreateInfo);

	// TODO: Load scene file
	hri::Scene scene = hri::Scene();
	printf("Startup complete\n");

	while (!glfwWindowShouldClose(gWindow))
	{
		gFrameTimer.tick();

		// TODO: set up render loop

		glfwPollEvents();
	}

	printf("Shutting down\n");
	destroyWindow(gWindow);
	printf("Goodbye!\n");

	return 0;
}
