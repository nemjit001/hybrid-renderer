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
	printf("Loading scenefile [%s]\n", path);

	tinyobj::ObjReaderConfig readerConfig = tinyobj::ObjReaderConfig{};
	readerConfig.triangulate = true;

	tinyobj::ObjReader reader;
	if (false == reader.ParseFromFile(path, readerConfig))
	{
		FATAL_ERROR("Failed to parse scene file");
	}

	const tinyobj::attrib_t& attributes = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
	const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

	// TODO: load obj PBR material into HRI material struct
	for (auto const& material : materials)
	{
		printf("Material: %s\n", material.name.c_str());

		printf("\tValues:\n");
		printf("\t\tAmbient Color:  [%.2f %.2f %.2f]\n", material.ambient[0], material.ambient[1], material.ambient[2]);
		printf("\t\tDiffuse Color:  [%.2f %.2f %.2f]\n", material.diffuse[0], material.diffuse[1], material.diffuse[2]);
		printf("\t\tSpecular:       [%.2f %.2f %.2f]\n", material.specular[0], material.specular[1], material.specular[2]);
		printf("\t\tTransmittance:  [%.2f %.2f %.2f]\n", material.transmittance[0], material.transmittance[1], material.transmittance[2]);
		printf("\t\tEmission Color: [%.2f %.2f %.2f]\n", material.emission[0], material.emission[1], material.emission[2]);
		printf("\t\tShininess:      [%.2f]\n", material.shininess);
		printf("\t\tIoR:            [%.2f]\n", material.ior);
		printf("\t\tDissolve:       [%.2f]\n", material.dissolve);

		printf("\tPBR extension:\n");
		printf("\t\tRoughness:           [%.2f]\n", material.roughness);
		printf("\t\tMetallic:            [%.2f]\n", material.metallic);
		printf("\t\tSheen:               [%.2f]\n", material.sheen);
		printf("\t\tClearcoat Thickness: [%.2f]\n", material.clearcoat_thickness);
		printf("\t\tClearcoar Roughness: [%.2f]\n", material.clearcoat_roughness);
		printf("\t\tAnisotropy:          [%.2f]\n", material.anisotropy);
		printf("\t\tAnisotropy Rotation: [%.2f]\n", material.anisotropy_rotation);

		printf("\tTextures:\n");
		printf("\t\tDiffuse Texture: [%s]\n", material.diffuse_texname.c_str());
		printf("\t\tNormal Texture:  [%s]\n", material.bump_texname.c_str());
	}

	// TODO: Load shape w/ associated material
	for (auto const& shape : shapes)
	{
		printf("Shape: %s\n", shape.name.c_str());
		printf("\t%zu indices\n", shape.mesh.indices.size());
	}

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
