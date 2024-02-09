#define TINYOBJLOADER_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <iostream>
#include <tiny_obj_loader.h>

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

hri::Scene loadScene(const char* path)
{
#if		DEMO_DEBUG_OUTPUT == 1
	printf("Loading scenefile [%s]\n", path);
#endif

	tinyobj::ObjReaderConfig readerConfig = tinyobj::ObjReaderConfig{};
	readerConfig.triangulate = true;

	tinyobj::ObjReader reader;
	if (false == reader.ParseFromFile(path, readerConfig))
	{
		FATAL_ERROR("Failed to parse scene file");
	}

	// Obj scene file attributes
	const tinyobj::attrib_t& objAttributes = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& objShapes = reader.GetShapes();
	const std::vector<tinyobj::material_t>& objMaterials = reader.GetMaterials();

	// Actually loaded attributes
	std::vector<hri::Material> materials; materials.reserve(objMaterials.size());

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
		
#if		DEMO_DEBUG_OUTPUT == 1
		printf("Material: %s\n", material.name.c_str());

		printf("\tMaterial Data:\n");
		printf("\t\tDiffuse:       [%.2f %.2f %.2f]\n", newMaterial.diffuse.r, newMaterial.diffuse.g, newMaterial.diffuse.b);
		printf("\t\tSpecular:      [%.2f %.2f %.2f]\n", newMaterial.specular.r, newMaterial.specular.g, newMaterial.specular.b);
		printf("\t\tTransmittance: [%.2f %.2f %.2f]\n", newMaterial.transmittance.r, newMaterial.transmittance.g, newMaterial.transmittance.b);
		printf("\t\tEmission:      [%.2f %.2f %.2f]\n", newMaterial.emission.r, newMaterial.emission.g, newMaterial.emission.b);
		printf("\t\tShininess:     [%.2f]\n", newMaterial.shininess);
		printf("\t\tIoR:           [%.2f]\n", newMaterial.ior);

		printf("\tTextures:\n");
		printf("\t\tDiffuse Texture: [%s]\n", material.diffuse_texname.c_str());
		printf("\t\tNormal Texture:  [%s]\n", material.bump_texname.c_str());
#endif
	}

	// Load shapes into hri compatible format
	for (auto const& shape : objShapes)
	{
		printf("Shape: %s\n", shape.name.c_str());
		printf("\t%zu indices\n", shape.mesh.indices.size());
	}

#if		DEMO_DEBUG_OUTPUT == 1
	printf("Loaded scene file!\n");
#endif

	return hri::Scene();
}

int main()
{
	WindowCreateInfo windowCreateInfo = WindowCreateInfo{};
	windowCreateInfo.width = SCR_WIDTH;
	windowCreateInfo.height = SCR_HEIGHT;
	windowCreateInfo.pTitle = DEMO_WINDOW_NAME;
	gWindow = initWindow(&windowCreateInfo);

	// TODO: Load scene file instead of manual setup
	hri::Scene scene = loadScene("assets/test_scene.obj");
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
