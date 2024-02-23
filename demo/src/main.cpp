#define TINYOBJLOADER_IMPLEMENTATION

#include <iostream>
#include <tiny_obj_loader.h>
#include <imgui.h>

#include "demo.h"
#include "renderer.h"
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

hri::Scene loadScene(hri::RenderContext& ctx, const char* path)
{
	printf("Loading scenefile [%s]\n", path);

	// Only real configuration needed is the triangulation on load
	tinyobj::ObjReaderConfig readerConfig = tinyobj::ObjReaderConfig{};
	readerConfig.triangulate = true;
	readerConfig.triangulation_method = "earcut";

	// Parse file & handle errors / report warnings
	tinyobj::ObjReader reader;
	if (false == reader.ParseFromFile(path, readerConfig))
	{
		if (reader.Error().size() > 0)
		{
			fprintf(stderr, "Loader Error: %s", reader.Error().c_str());
		}

		FATAL_ERROR("Failed to parse scene file");
	}

	if (reader.Warning().size() > 0)
	{
		printf("Loader Warning: %s", reader.Warning().c_str());
	}

	// Obj scene file attributes
	const tinyobj::attrib_t& objAttributes = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& objShapes = reader.GetShapes();
	const std::vector<tinyobj::material_t>& objMaterials = reader.GetMaterials();

	// Actually loaded attributes
	std::vector<hri::Material> materials; materials.reserve(objMaterials.size());
	std::vector<hri::Mesh> meshes; meshes.reserve(objShapes.size());

	// For now assume all nodes are meshes (i.e. no instancing or per node transform)
	std::vector<hri::SceneNode> sceneNodes; sceneNodes.reserve(objShapes.size());

	// Load material data into hri compatible format
	for (auto const& material : objMaterials)
	{
		// TODO: load texture files if a material contains texuture maps
		hri::Material newMaterial = hri::Material{
			hri::Float3(material.diffuse[0], material.diffuse[1], material.diffuse[2]),
			hri::Float3(material.specular[0], material.specular[1], material.specular[2]),
			hri::Float3(material.transmittance[0], material.transmittance[1], material.transmittance[2]),
			hri::Float3(material.emission[0], material.emission[1], material.emission[2]),
			material.shininess,
			material.ior,
		};

		materials.push_back(newMaterial);
	}

	// Load shapes into hri compatible format
	for (auto const& shape : objShapes)
	{
		// Retrieve indexed vertices for Shape
		std::vector<hri::Vertex> vertices; vertices.reserve(shape.mesh.indices.size());
		std::vector<uint32_t> indices; indices.reserve(shape.mesh.indices.size());
		for (auto const& index : shape.mesh.indices)
		{
			size_t vertexIndex = index.vertex_index * 3;
			size_t normalIndex = index.normal_index * 3;
			size_t texCoordIndex = index.texcoord_index * 2;

			hri::Vertex newVertex = hri::Vertex{
				hri::Float3(
					objAttributes.vertices[vertexIndex + 0],
					objAttributes.vertices[vertexIndex + 1],
					objAttributes.vertices[vertexIndex + 2]
				),
				hri::Float3(
					objAttributes.normals[normalIndex + 0],
					objAttributes.normals[normalIndex + 1],
					objAttributes.normals[normalIndex + 2]
				),
				hri::Float3(0.0f),	// The tangent vector is initialy (0, 0, 0), because it can only be calculated AFTER triangles are loaded
				hri::Float2(
					objAttributes.texcoords[texCoordIndex + 0],
					objAttributes.texcoords[texCoordIndex + 1]
				),
			};

			vertices.push_back(newVertex);
			indices.push_back(static_cast<uint32_t>(indices.size()));	// Works because mesh is already triangulated on load
		}

		// Calculate tangent space vectors for Shape
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			hri::Vertex& v0 = vertices[indices[i + 0]];
			hri::Vertex& v1 = vertices[indices[i + 1]];
			hri::Vertex& v2 = vertices[indices[i + 2]];

			const hri::Float3 e1 = v1.position - v0.position, e2 = v2.position - v0.position;
			const hri::Float2 dUV1 = v1.textureCoord - v0.textureCoord, dUV2 = v2.textureCoord - v0.textureCoord;

			const float f = 1.0f / (dUV1.x * dUV2.y - dUV2.x * dUV1.y);
			const hri::Float3 tangent = normalize(f * (dUV2.y * e1 - dUV1.y * e2));

			// Ensure tangent is 90 degrees w/ normal for each vertex
			v0.tangent = normalize(tangent - dot(v0.normal, tangent) * v0.normal);
			v1.tangent = normalize(tangent - dot(v1.normal, tangent) * v1.normal);
			v2.tangent = normalize(tangent - dot(v2.normal, tangent) * v2.normal);
		}

		// Add scene node with mesh & material data
		uint32_t materialIdx = static_cast<uint32_t>(shape.mesh.material_ids[0]);	// Assumes all faces share the same materials (maybe naive?)
		uint32_t meshIdx = static_cast<uint32_t>(meshes.size());
		sceneNodes.push_back(hri::SceneNode{ meshIdx, materialIdx });

		meshes.push_back(std::move(hri::Mesh(ctx, vertices, indices)));
	}

	printf("Loaded scene file:\n");
	printf("\t%zu materials\n", materials.size());
	printf("\t%zu meshes\n", meshes.size());

	struct hri::SceneData sceneData = hri::SceneData{
		std::move(meshes),
		std::move(materials),
	};

	return hri::Scene(
		hri::SceneParameters{},
		std::move(sceneData),
		sceneNodes
	);
}

void drawStatusWindow(float deltaTime)
{
	static float AVG_FRAMETIME = 1.0f;
	static float ALPHA = 1.0f;

	AVG_FRAMETIME = (1.0f - ALPHA) * AVG_FRAMETIME + ALPHA * deltaTime;
	if (ALPHA > 0.05f) ALPHA *= 0.5f;

	if (ImGui::Begin("Status"))
	{
		ImGui::SetWindowSize(ImVec2(300.0f, 250.0f), ImGuiCond_FirstUseEver);
		ImGui::Text("Resolution: %d x %d", gDisplayWidth, gDisplayHeight);
		ImGui::Text("Frame Time: %8.2f ms", AVG_FRAMETIME * 1'000.0f);
		ImGui::Text("FPS:        %8.2f fps", 1.0f / AVG_FRAMETIME);
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
	camera.updateMatrices();
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
	ctxCreateInfo.instanceExtensions = {};
	ctxCreateInfo.deviceExtensions = {
   		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
   		// VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
   		// VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
   		// VK_KHR_RAY_QUERY_EXTENSION_NAME,
	};

	hri::RenderContext renderContext = hri::RenderContext(ctxCreateInfo);

	// Set up world cam
	hri::Camera camera = hri::Camera(
		hri::CameraParameters{
			60.0f,
			static_cast<float>(gDisplayWidth) / static_cast<float>(gDisplayHeight),
		},
		hri::Float3(0.0f, 1.0f, -5.0f),
		hri::Float3(0.0f, 1.0f, 0.0f)
	);

	// Load scene file
	hri::Scene scene = loadScene(renderContext, "assets/test_scene.obj");

	// Create renderer
	Renderer renderer = Renderer(renderContext, camera, scene);

	printf("Startup complete\n");

	while (!windowManager.windowShouldClose(gWindow))
	{
		windowManager.pollEvents();
		gFrameTimer.tick();

		if (windowManager.isWindowMinimized(gWindow))
			continue;

		// Record ImGUI draw commands
		uiManager.startDraw();
		drawStatusWindow(gFrameTimer.deltaTime);
		uiManager.endDraw();

		// Draw renderer frame
		renderer.drawFrame();

		// Update camera state
		if (gWindowResized)
			camera.parameters.aspectRatio = static_cast<float>(gDisplayWidth) / static_cast<float>(gDisplayHeight);

		handleCameraInput(gWindow, gFrameTimer.deltaTime, camera);
		
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
