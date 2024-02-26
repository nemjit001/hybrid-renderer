#define TINYOBJLOADER_IMPLEMENTATION

#include <iostream>
#include <tiny_obj_loader.h>
#include <imgui.h>

#include "demo.h"
#include "detail/raytracing.h"
#include "material.h"
#include "renderer.h"
#include "scene.h"
#include "timer.h"
#include "ui_manager.h"
#include "window_manager.h"

#if DEMO_DEBUG == 1 && HRI_PLATFORM_WINDOWS == 1
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

static Timer gFrameTimer		= Timer();
static WindowHandle* gWindow	= nullptr;

static bool gWindowResized  = false;
static int gDisplayWidth 	= SCR_WIDTH;
static int gDisplayHeight 	= SCR_HEIGHT;

void drawSceneGraphNodeMenu(std::vector<SceneNode>& nodes)
{
	for (auto& node : nodes)
	{
		if (ImGui::TreeNode(node.name.c_str()))
		{
			ImGui::DragFloat3("Position", node.transform.position.xyz, 0.05f);
			ImGui::DragFloat3("Rotation", node.transform.rotation.xyz, 0.5f);
			ImGui::DragFloat3("Scale", node.transform.scale.xyz, 0.05f);
			ImGui::TreePop();
		}
	}
}

void drawConfigWindow(float deltaTime, hri::Camera& camera, SceneGraph& scene)
{
	static float AVG_FRAMETIME = 1.0f;
	static float ALPHA = 1.0f;

	AVG_FRAMETIME = (1.0f - ALPHA) * AVG_FRAMETIME + ALPHA * deltaTime;
	if (ALPHA > 0.05f) ALPHA *= 0.5f;

	if (ImGui::Begin("Config"))
	{
		ImGui::SetWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
		ImGui::SetWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_FirstUseEver);

		ImGui::SeparatorText("Status");
		ImGui::Text("Resolution: %d x %d", gDisplayWidth, gDisplayHeight);
		ImGui::Text("Frame Time: %8.2f ms", AVG_FRAMETIME * 1'000.0f);
		ImGui::Text("FPS:        %8.2f fps", 1.0f / AVG_FRAMETIME);

		ImGui::SeparatorText("Scene");
		ImGui::DragFloat("LOD Near", &scene.parameters.nearPoint, 0.5f);
		ImGui::DragFloat("LOD Far", &scene.parameters.farPoint, 0.5f);

		ImGui::SeparatorText("Camera");
		ImGui::Text("Position: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
		ImGui::Text("Forward:  %.2f %.2f %.2f", camera.forward.x, camera.forward.y, camera.forward.z);
		ImGui::DragFloat("FOV Y", &camera.parameters.fovYDegrees, 1.0f);
		ImGui::DragFloat("Near Plane", &camera.parameters.zNear, 0.05f);
		ImGui::DragFloat("Far Plane", &camera.parameters.zFar, 0.05f);

		ImGui::SeparatorText("Scene Nodes");
		drawSceneGraphNodeMenu(scene.nodes);
	}

	ImGui::End();
}

void windowResizeCallback(WindowHandle* window, int width, int height)
{
	gDisplayWidth = width;
	gDisplayHeight = height;
	gWindowResized = true;
}

void handleCameraInput(WindowHandle* window, float deltaTime, hri::Camera& camera)
{
	bool cameraUpdated = false;

	hri::Float3 forward = camera.forward;
	hri::Float3 right = camera.right;
	hri::Float3 up = camera.up;

	hri::Float3 positionDelta = hri::Float3(0.0f);
	
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) positionDelta += forward * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) positionDelta -= forward * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) positionDelta += right * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) positionDelta -= right * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) positionDelta += up * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) positionDelta -= up * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true;

	camera.position += positionDelta;
	hri::Float3 target = camera.position + forward;

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) target += up * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) target -= up * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) target += right * CAMERA_SPEED * deltaTime; cameraUpdated = true;
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) target -= right * CAMERA_SPEED * deltaTime; cameraUpdated = true;

	if (!cameraUpdated)
		return;

	camera.forward = hri::normalize(target - camera.position);
	camera.right = hri::normalize(hri::cross(HRI_WORLD_UP, camera.forward));
	camera.up = hri::normalize(hri::cross(camera.forward, camera.right));
}

int main()
{
#if DEMO_DEBUG == 1 && HRI_PLATFORM_WINDOWS == 1
	// Enable debug memory leak detection on windows
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Set up window manager & main window
	WindowManager windowManager = WindowManager();

	WindowCreateInfo windowCreateInfo = WindowCreateInfo{};
	windowCreateInfo.width = SCR_WIDTH;
	windowCreateInfo.height = SCR_HEIGHT;
	windowCreateInfo.pTitle = DEMO_WINDOW_NAME;
	windowCreateInfo.resizable = true;
	windowCreateInfo.pfnResizeFunc = windowResizeCallback;
	gWindow = windowManager.createWindow(&windowCreateInfo);

	// Set up UI manager
	UIManager uiManager = UIManager(gWindow);

	// Set up render context
	hri::RenderContextCreateInfo ctxCreateInfo = hri::RenderContextCreateInfo{};
	ctxCreateInfo.appName = DEMO_APP_NAME;
	ctxCreateInfo.appVersion = DEMO_APP_VERSION;
	ctxCreateInfo.surfaceCreateFunc = [](VkInstance instance, VkSurfaceKHR* surface) { return WindowManager::createVulkanSurface(instance, gWindow, nullptr, surface); };
	ctxCreateInfo.vsyncMode = hri::VSyncMode::Disabled;

	// Set required extensions
	ctxCreateInfo.instanceExtensions = {};
	ctxCreateInfo.deviceExtensions = {
   		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
   		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
   		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
   		VK_KHR_RAY_QUERY_EXTENSION_NAME,
	};

	// Enable required features
	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = VkPhysicalDeviceRayQueryFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	rayQueryFeatures.rayQuery = true;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = VkPhysicalDeviceAccelerationStructureFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	accelerationStructureFeatures.accelerationStructure = true;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = VkPhysicalDeviceRayTracingPipelineFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
	rtPipelineFeatures.rayTracingPipeline = true;

	ctxCreateInfo.deviceFeatures12.bufferDeviceAddress = true;
	ctxCreateInfo.deviceFeatures12.descriptorIndexing = true;
	ctxCreateInfo.deviceFeatures13.synchronization2 = true;
	ctxCreateInfo.extensionFeatures = {
		rayQueryFeatures,
		accelerationStructureFeatures,
		rtPipelineFeatures,
	};

	hri::RenderContext renderContext = hri::RenderContext(ctxCreateInfo);
	raytracing::RayTracingContext rtContext = raytracing::RayTracingContext(renderContext);

	// Set up world cam
	hri::Camera camera = hri::Camera(
		hri::CameraParameters{
			60.0f,
			static_cast<float>(gDisplayWidth) / static_cast<float>(gDisplayHeight),
		},
		hri::Float3(0.0f, 1.0f, -5.0f),
		hri::Float3(0.0f, 1.0f, 0.0f)
	);

	// Load scene file & Create renderer
	SceneGraph scene = SceneLoader::load(rtContext, "./assets/default_scene.json");
	Renderer renderer = Renderer(rtContext, camera, scene);

	printf("Startup complete\n");

	while (!windowManager.windowShouldClose(gWindow))
	{
		windowManager.pollEvents();
		gFrameTimer.tick();

		if (windowManager.isWindowMinimized(gWindow))
			continue;

		// Record ImGUI draw commands
		uiManager.startDraw();
		drawConfigWindow(gFrameTimer.deltaTime, camera, scene);
		uiManager.endDraw();

		// Update scene & draw renderer frame
		scene.update(gFrameTimer.deltaTime);
		renderer.drawFrame();

		// Update camera state
		if (gWindowResized)
			camera.parameters.aspectRatio = static_cast<float>(gDisplayWidth) / static_cast<float>(gDisplayHeight);

		handleCameraInput(gWindow, gFrameTimer.deltaTime, camera);
		camera.updateMatrices();
		
		if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			windowManager.closeWindow(gWindow);

		// Always set to false, resize dependent things should be handled here
		gWindowResized = false;
	}

	printf("Shutting down\n");
	windowManager.destroyWindow(gWindow);

	printf("Goodbye!\n");
	return 0;
}
