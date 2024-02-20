#define TINYOBJLOADER_IMPLEMENTATION

#include <iostream>
#include <tiny_obj_loader.h>

#include "demo.h"
#include "subsystems.h"
#include "timer.h"
#include "window_manager.h"

static Timer gFrameTimer		= Timer();
static WindowHandle* gWindow	= nullptr;

hri::Scene loadScene(const char* path)
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

		meshes.push_back(hri::Mesh(vertices, indices));
	}

	printf("Loaded scene file:\n");
	printf("\t%zu materials\n", materials.size());
	printf("\t%zu meshes\n", meshes.size());

	struct hri::SceneData sceneData = hri::SceneData{
		meshes,
		materials,
	};

	return hri::Scene(
		hri::SceneParameters{},
		sceneData,
		sceneNodes
	);
}

int main()
{
	// Set up window manager & main window
	WindowManager windowManager = WindowManager();

	WindowCreateInfo windowCreateInfo = WindowCreateInfo{};
	windowCreateInfo.width = SCR_WIDTH;
	windowCreateInfo.height = SCR_HEIGHT;
	windowCreateInfo.pTitle = DEMO_WINDOW_NAME;
	windowCreateInfo.resizable = true;
	gWindow = windowManager.createWindow(&windowCreateInfo);

	// Set up render context
	hri::RenderContextCreateInfo ctxCreateInfo = hri::RenderContextCreateInfo{};
	ctxCreateInfo.surfaceCreateFunc = [](VkInstance instance, VkSurfaceKHR* surface) { return WindowManager::createVulkanSurface(instance, gWindow, nullptr, surface); };
	ctxCreateInfo.vsyncMode = hri::VSyncMode::Disabled;
	hri::RenderContext renderContext = hri::RenderContext(ctxCreateInfo);

	// Create render core, shader database, subsystem manager, and descriptor set allocator
	hri::RenderCore renderCore = hri::RenderCore(&renderContext);
	hri::ShaderDatabase shaderDB = hri::ShaderDatabase(&renderContext);
	hri::RenderSubsystemManager subsystemManager = hri::RenderSubsystemManager(&renderContext);
	hri::DescriptorSetAllocator descriptorSetAllocator = hri::DescriptorSetAllocator(&renderContext);

	// Initialize shader database w/ all shaders
	shaderDB.registerShader("PresentVert", hri::Shader::loadFile(&renderContext, "./shaders/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	shaderDB.registerShader("StaticVert", hri::Shader::loadFile(&renderContext, "./shaders/static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	shaderDB.registerShader("GBufferLayoutFrag", hri::Shader::loadFile(&renderContext, "./shaders/gbuffer_layout.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
	shaderDB.registerShader("PresentFrag", hri::Shader::loadFile(&renderContext, "./shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

	// Set up render pass builders, attachments, and resource managers
	hri::RenderPassBuilder gbufferLayoutPassBuilder = hri::RenderPassBuilder(&renderContext)
		.addAttachment(
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE
		)
		.addAttachment(
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE
		)
		.addAttachment(
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE
		)
		.addAttachment(
			VK_FORMAT_D32_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE
		)
		.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
		.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
		.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL })
		.setAttachmentReference(hri::AttachmentType::DepthStencil, VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL });

	hri::RenderPassBuilder presentPassBuilder = hri::RenderPassBuilder(&renderContext)
		.addAttachment(
			renderContext.swapFormat(),
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE
		)
		.setAttachmentReference(hri::AttachmentType::Color,	VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	std::vector<hri::RenderAttachmentConfig> gbufferAttachmentConfigs = {
		hri::RenderAttachmentConfig{ VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
		hri::RenderAttachmentConfig{ VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
		hri::RenderAttachmentConfig{ VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT },
		hri::RenderAttachmentConfig{ VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT },
	};

	hri::RenderPassResourceManager gbufferLayoutPassManager = hri::RenderPassResourceManager(
		&renderContext,
		gbufferLayoutPassBuilder.build(),
		gbufferAttachmentConfigs
	);

	hri::SwapchainPassResourceManager presentPassManager = hri::SwapchainPassResourceManager(
		&renderContext,
		presentPassBuilder.build(),
		{}	// No extra attachments for swapchain pass
	);

	// Set clear values for pass attachments
	gbufferLayoutPassManager.setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });
	gbufferLayoutPassManager.setClearValue(1, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });
	gbufferLayoutPassManager.setClearValue(2, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });
	gbufferLayoutPassManager.setClearValue(3, VkClearValue{ { 1.0f, 0x00 } });
	presentPassManager.setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });

	// Set up render subsystems & register with render manager
	GBufferLayoutSubsystem gbufferLayoutSystem = GBufferLayoutSubsystem(&renderContext, &descriptorSetAllocator, &shaderDB, gbufferLayoutPassManager.renderPass());
	PresentationSubsystem presentationSystem = PresentationSubsystem(&renderContext, &descriptorSetAllocator, &shaderDB, presentPassManager.renderPass());

	subsystemManager.registerSubsystem("GBufferLayoutSystem", &gbufferLayoutSystem);
	subsystemManager.registerSubsystem("PresentationSystem", &presentationSystem);

	// Register a callback for when the swap chain is invalidated (recreates swap dependent resources for render passes)
	renderCore.setOnSwapchainInvalidateCallback([&gbufferLayoutPassManager, &presentPassManager](vkb::Swapchain _swapchain) {
		gbufferLayoutPassManager.recreateResources();
		presentPassManager.recreateResources();
	});

	// Load scene file & set up virtual camera
	hri::Scene scene = loadScene("assets/test_scene.obj");
	hri::Camera camera = hri::Camera(
		hri::CameraParameters{ 60.0f },
		hri::Float3(0.0f, 1.0f, -5.0f),
		hri::Float3(0.0f, 0.0f, 0.0f)
	);

	printf("Startup complete\n");

	while (!windowManager.windowShouldClose(gWindow))
	{
		windowManager.pollEvents();
		gFrameTimer.tick();

		if (windowManager.isWindowMinimized(gWindow))
			continue;

		renderCore.startFrame();

		hri::ActiveFrame frame = renderCore.getActiveFrame();
		frame.beginCommands();

		gbufferLayoutPassManager.beginRenderPass(frame);
		subsystemManager.recordSubsystem("GBufferLayoutSystem", frame);
		gbufferLayoutPassManager.endRenderPass(frame);

		presentPassManager.beginRenderPass(frame);
		subsystemManager.recordSubsystem("PresentationSystem", frame);
		presentPassManager.endRenderPass(frame);

		frame.endCommands();
		renderCore.endFrame();
	}

	printf("Shutting down\n");
	renderCore.awaitFrameFinished();
	windowManager.destroyWindow(gWindow);

	printf("Goodbye!\n");
	return 0;
}
