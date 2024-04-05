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

#if USE_BENCHMARK_SCENE == 1

const int32_t gBenchMarkMeshCount			= 20;
const char* gBenchMarkMesh					= "assets/armadillo_lod.obj";
const char* gBenchMarkLods[MAX_LOD_LEVELS]	= { "armadillo01", "armadillo05", "armadillo10" };

const uint32_t gCaptureFrameCount = 500;
std::vector<hri::Float3> gCamPositions = {
	hri::Float3( 60.0f, 10.0f,  60.0f),
	hri::Float3(-60.0f, 10.0f,  60.0f),
	hri::Float3( 60.0f, 10.0f, -60.0f),
	hri::Float3(-60.0f, 10.0f, -60.0f),
	hri::Float3( 60.0f,  1.0f,  60.0f),
	hri::Float3(-60.0f,  1.0f,  60.0f),
	hri::Float3( 60.0f,  1.0f, -60.0f),
	hri::Float3(-60.0f,  1.0f, -60.0f),

	hri::Float3( 20.0f, 10.0f,  20.0f),
	hri::Float3(-20.0f, 10.0f,  20.0f),
	hri::Float3( 20.0f, 10.0f, -20.0f),
	hri::Float3(-20.0f, 10.0f, -20.0f),
	hri::Float3( 20.0f,  1.0f,  20.0f),
	hri::Float3(-20.0f,  1.0f,  20.0f),
	hri::Float3( 20.0f,  1.0f, -20.0f),
	hri::Float3(-20.0f,  1.0f, -20.0f),
};

#endif

static Timer gFrameTimer		= Timer();
static WindowHandle* gWindow	= nullptr;

static bool gWindowResized  = false;
static int gDisplayWidth 	= SCR_WIDTH;
static int gDisplayHeight 	= SCR_HEIGHT;

#if USE_BENCHMARK_SCENE == 1
SceneGraph loadBenchmarkScene(raytracing::RayTracingContext& ctx)
{
	std::srand(0x42);
	std::vector<Material> materials = {};
	std::vector<hri::Mesh> meshes = {};
	std::vector<SceneNode> nodes = {};

	// Load benchmark mesh LODs
	for (size_t i = 0; i < MAX_LOD_LEVELS; i++)
	{
		Material material = Material{};
		std::vector<hri::Vertex> verts;
		std::vector<uint32_t> indices;
		if (SceneLoader::loadOBJMesh(gBenchMarkMesh, gBenchMarkLods[i], material, verts, indices, (i == 0)))
			meshes.push_back(std::move(hri::Mesh(ctx.renderContext, verts, indices, MESH_RAYTRACING_BUFFER_FLAGS)));

		if (i == 0)
		{
			material.diffuse = hri::Float3(0.1f);
			materials.push_back(material);
		}
	}

	{	// Load an area light and a floor mesh
		Material material = Material{};
		std::vector<hri::Vertex> verts;
		std::vector<uint32_t> indices;
		if (SceneLoader::loadOBJMesh("assets/simple_objects_lod.obj", "Plane_LOD0", material, verts, indices, false))
			meshes.push_back(std::move(hri::Mesh(ctx.renderContext, verts, indices, MESH_RAYTRACING_BUFFER_FLAGS)));

		materials.push_back(material);

		// HACK: temp second load, remove when instance data setting in scene works
		verts.clear(); indices.clear();
		if (SceneLoader::loadOBJMesh("assets/simple_objects_lod.obj", "Plane_LOD0", material, verts, indices, true))
			meshes.push_back(std::move(hri::Mesh(ctx.renderContext, verts, indices, MESH_RAYTRACING_BUFFER_FLAGS)));

		materials.push_back(material);

		SceneNode floorNode = SceneNode{};
		floorNode.name = "Floor";
		floorNode.transform = SceneTransform{};
		floorNode.transform.position = hri::Float3(0.0f, 0.0f, 0.0f);
		floorNode.transform.scale = hri::Float3(100.0f, 1.0f, 100.0f);
		floorNode.material = 1;
		floorNode.numLods = 1;
		floorNode.meshLODs[0] = 4;

		SceneNode lightNode = SceneNode{};
		lightNode.name = "Area Light";
		lightNode.transform = SceneTransform{};
		lightNode.transform.position = hri::Float3(0.0f, 20.0f, 0.0f);
		lightNode.transform.scale = hri::Float3(15.0f, 1.0f, 15.0f);
		lightNode.material = 2;
		lightNode.numLods = 1;
		lightNode.meshLODs[0] = 3;

		nodes.push_back(floorNode);
		nodes.push_back(lightNode);
	}

	// Set up benchmark nodes
	for (int32_t x = -(gBenchMarkMeshCount / 2); x < (gBenchMarkMeshCount / 2); x++)
	{
		for (int32_t y = -(gBenchMarkMeshCount / 2); y < (gBenchMarkMeshCount / 2); y++)
		{
			char buff[64];
			snprintf(buff, 64, "(%d %d)", x, y);

			SceneNode newNode = SceneNode{};
			newNode.name = "Armadillo" + std::string(buff);
			newNode.transform = SceneTransform{};
			newNode.transform.position = hri::Float3((float)x * 5.0f, 1.3f, (float)y * 5.0f);
			newNode.transform.scale = hri::Float3(0.025f, 0.025f, 0.025f);
			newNode.transform.rotation.y = (static_cast <float>(rand()) / static_cast <float>(RAND_MAX)) * 360.0f;
			newNode.material = 0;
			newNode.numLods = 3;
			newNode.meshLODs[0] = 0;
			newNode.meshLODs[1] = 1;
			newNode.meshLODs[2] = 2;

			nodes.push_back(newNode);
		}
	}

	return SceneGraph(ctx, std::move(materials), std::move(meshes), std::move(nodes));
}
#endif

bool drawSceneGraphNodeMenu(std::vector<SceneNode>& nodes)
{
	bool updated = false;

	for (auto& node : nodes)
	{
		if (ImGui::TreeNode(node.name.c_str()))
		{
			updated |= ImGui::DragFloat3("Position", node.transform.position.xyz, 0.05f);
			updated |= ImGui::DragFloat3("Rotation", node.transform.rotation.xyz, 0.5f);
			updated |= ImGui::DragFloat3("Scale", node.transform.scale.xyz, 0.05f);
			ImGui::TreePop();
		}
	}

	return updated;
}

bool drawConfigWindow(float deltaTime, Renderer& renderer, hri::Camera& camera, SceneGraph& scene)
{
	static float AVG_FRAMETIME = 1.0f;
	static float ALPHA = 1.0f;

	AVG_FRAMETIME = (1.0f - ALPHA) * AVG_FRAMETIME + ALPHA * deltaTime;
	if (ALPHA > 0.05f) ALPHA *= 0.5f;

	bool updated = false;
	if (ImGui::Begin("Config"))
	{
		ImGui::SetWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
		ImGui::SetWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Always);

		ImGui::SeparatorText("Status");
		ImGui::Text("Resolution: %d x %d", gDisplayWidth, gDisplayHeight);
		ImGui::Text("Frame Time: %8.2f ms", AVG_FRAMETIME * 1'000.0f);
		ImGui::Text("FPS:        %8.2f fps", 1.0f / AVG_FRAMETIME);

		ImGui::SeparatorText("Renderer");
		static bool test;
		updated |= ImGui::Checkbox("Use reference Path Tracer", &renderer.usePathTracer);
		updated |= ImGui::Checkbox("Use temporal accumulation", &renderer.useTemporalAccumulation);

		ImGui::SeparatorText("Scene");
		updated |= ImGui::DragFloat("LOD Bias", &scene.parameters.lodBias, 0.01f);
		updated |= ImGui::DragFloat("LOD T Interval", &scene.parameters.transitionInterval, 0.01f, 0.0f, 1.0f);
		updated |= ImGui::DragFloat("LOD Z Near", &scene.parameters.nearPoint, 0.5f);
		updated |= ImGui::DragFloat("LOD Z Far", &scene.parameters.farPoint, 0.5f);

		ImGui::SeparatorText("Camera");
		ImGui::Text("Position: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
		ImGui::Text("Forward:  %.2f %.2f %.2f", camera.forward.x, camera.forward.y, camera.forward.z);
		updated |= ImGui::DragFloat("FOV Y", &camera.parameters.fovYDegrees, 1.0f);
		updated |= ImGui::DragFloat("Near Plane", &camera.parameters.zNear, 0.05f);
		updated |= ImGui::DragFloat("Far Plane", &camera.parameters.zFar, 0.05f);

		ImGui::SeparatorText("Scene Nodes");
		updated |= drawSceneGraphNodeMenu(scene.nodes);
	}

	ImGui::End();
	return updated;
}

void windowResizeCallback(WindowHandle* window, int width, int height)
{
	gDisplayWidth = width;
	gDisplayHeight = height;
	gWindowResized = true;
}

bool handleCameraInput(WindowHandle* window, float deltaTime, hri::Camera& camera)
{
	bool cameraUpdated = false;

	hri::Float3 forward = camera.forward;
	hri::Float3 right = camera.right;
	hri::Float3 up = camera.up;

	hri::Float3 positionDelta = hri::Float3(0.0f);
	
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { positionDelta += forward * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { positionDelta -= forward * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { positionDelta += right * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { positionDelta -= right * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { positionDelta += up * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { positionDelta -= up * 2.0f * CAMERA_SPEED * deltaTime; cameraUpdated = true; }

	camera.position += positionDelta;
	hri::Float3 target = camera.position + forward;

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) { target += up * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) { target -= up * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) { target += right * CAMERA_SPEED * deltaTime; cameraUpdated = true; }
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) { target -= right * CAMERA_SPEED * deltaTime; cameraUpdated = true; }

	if (!cameraUpdated)
		return false;

	camera.forward = hri::normalize(target - camera.position);
	camera.right = hri::normalize(hri::cross(HRI_WORLD_UP, camera.forward));
	camera.up = hri::normalize(hri::cross(camera.forward, camera.right));
	return true;
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

	ctxCreateInfo.deviceFeatures.shaderInt64 = true;
	ctxCreateInfo.deviceFeatures12.hostQueryReset = true;
	ctxCreateInfo.deviceFeatures12.bufferDeviceAddress = true;
	ctxCreateInfo.deviceFeatures12.descriptorIndexing = true;
	ctxCreateInfo.deviceFeatures12.scalarBlockLayout = true;
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
#if USE_BENCHMARK_SCENE == 1
	SceneGraph scene = loadBenchmarkScene(rtContext);
#else
	SceneGraph scene = SceneLoader::load(rtContext, "./assets/interior_scene.json");
#endif
	Renderer renderer = Renderer(rtContext, camera, scene);

	printf("Startup complete\n");

#if DO_BENCHMARK == 1
	renderer.usePathTracer = BENCHMARK_PATH_TRACER;
	scene.parameters.transitionInterval = BENCHMARK_T_INTERVAL;

	printf("Running benchmark (Path Tracing %d, T: %5.2f)\n", renderer.usePathTracer, scene.parameters.transitionInterval);
	uint32_t positionIndex = 0;
	uint32_t frameIndex = 0;
#endif

	while (!windowManager.windowShouldClose(gWindow))
	{
#if DO_BENCHMARK == 1
		if (frameIndex % gCaptureFrameCount == 0)
		{
			if (positionIndex >= gCamPositions.size())
				break;

			printf("--- BENCHMARK POSITION %d ---\n", positionIndex);

			camera.position = gCamPositions[positionIndex];
			camera.forward = hri::normalize(hri::Float3(0.0f) - camera.position);
			camera.right = hri::normalize(hri::cross(HRI_WORLD_UP, camera.forward));
			camera.up = hri::normalize(hri::cross(camera.forward, camera.right));
			camera.parameters.aspectRatio = static_cast<float>(gDisplayWidth) / static_cast<float>(gDisplayHeight);
			camera.updateMatrices();
			positionIndex++;
		}
#endif

		windowManager.pollEvents();
		gFrameTimer.tick();

		if (windowManager.isWindowMinimized(gWindow))
			continue;

		// Record ImGUI draw commands
		uiManager.startDraw();
		bool UIUpdated = drawConfigWindow(gFrameTimer.deltaTime, renderer, camera, scene);
		uiManager.endDraw();

		// Update scene & draw renderer frame
		scene.update(gFrameTimer.deltaTime);
		renderer.prepareFrame();
		renderer.drawFrame();

		// Update camera state
		bool cameraUpdated = handleCameraInput(gWindow, gFrameTimer.deltaTime, camera);
		if (gWindowResized || UIUpdated || cameraUpdated)
		{
			camera.parameters.aspectRatio = static_cast<float>(gDisplayWidth) / static_cast<float>(gDisplayHeight);
			camera.updateMatrices();
		}
		
		if (glfwGetKey(gWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			windowManager.closeWindow(gWindow);

		// Always set to false, resize dependent things should be handled here
		gWindowResized = false;

#if DO_BENCHMARK == 1
		frameIndex++;
#endif
	}

	printf("Shutting down\n");
	windowManager.destroyWindow(gWindow);

	printf("Goodbye!\n");
	return 0;
}
