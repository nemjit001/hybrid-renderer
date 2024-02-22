project("hybrid-renderer-lib" VERSION 1.0.0)
set(LIB_NAME hybridrenderer)

# Set up dependencies
find_package(Vulkan)

option(VK_BOOTSTRAP_DISABLE_WARNINGS "Disable warnings during compilation" OFF)
option(VK_BOOTSTRAP_WERROR "Enable warnings as errors during compilation" OFF)
add_subdirectory("deps/vk-bootstrap/")

add_subdirectory("deps/VulkanMemoryAllocator/")

add_subdirectory("deps/glm/")

# Project setup
file(GLOB PROJECT_HEADERS "include/*.h" "include/**/*.h")
file(GLOB PROJECT_SOURCES "src/*.cpp" "src/**/*.cpp")
file(GLOB PROJECT_CONFIG "config.h.in")

# Project config variables
set(HRI_ENGINE_NAME "HybridRenderEngine")
set(HRI_FRAMES_IN_FLIGHT 3)

#Set up configuration file
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
	${CMAKE_CURRENT_SOURCE_DIR}/include/config.h
)

add_library(${LIB_NAME}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
	${PROJECT_SHADERS}
	${PROJECT_CONFIG}
)

target_include_directories(${LIB_NAME} PUBLIC
	"include/"
	${Vulkan_INCLUDE_DIRS}
	"deps/vk-bootstrap/src/"
	"deps/VulkanMemoryAllocator/include/"
	"deps/glm/"
)

target_link_libraries(${LIB_NAME}
	${Vulkan_LIBRARIES}
	vk-bootstrap
	VulkanMemoryAllocator
)
