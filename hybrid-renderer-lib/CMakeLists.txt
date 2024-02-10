project("hybrid-renderer-lib" VERSION 1.0.0)
set(LIB_NAME hybridrenderer)

# Set up dependencies
find_package(Vulkan)

option(VK_BOOTSTRAP_DISABLE_WARNINGS "Disable warnings during compilation" OFF)
option(VK_BOOTSTRAP_WERROR "Enable warnings as errors during compilation" OFF)
add_subdirectory("deps/vk-bootstrap/")

file(GLOB PROJECT_HEADERS "include/*.h" "include/**/*.h")
file(GLOB PROJECT_SOURCES "src/*.cpp" "src/**/*.cpp")

add_library(${LIB_NAME}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
)

target_include_directories(${LIB_NAME} PUBLIC
	"include/"
	${Vulkan_INCLUDE_DIRS}
	"deps/vk-bootstrap/src/"
)

target_link_libraries(${LIB_NAME}
	${Vulkan_LIBRARIES}
	vk-bootstrap
)
