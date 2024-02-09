project("hybrid-renderer-demo" VERSION 1.0.0)
set(TARGET_NAME hrdemo)

file(GLOB PROJECT_HEADERS "include/*.h")
file(GLOB PROJECT_SOURCES "src/*.cpp")
file(GLOB PROJECT_SHADERS "shaders/*.vert" "shaders/*.frag")

# Add dependencies
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory("deps/glfw/")

add_subdirectory("deps/tinyobjloader/")

add_executable(${TARGET_NAME}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
)

target_link_libraries(${TARGET_NAME} PRIVATE
	glfw ${GLFW_LIBRARIES}
	tinyobjloader
	hybridrenderer
)

target_include_directories(${TARGET_NAME} PRIVATE
	"include/"
	"/deps/glfw/include/"
	"deps/tinyobjloader/"
	"../hybrid-renderer-lib/include/"
)

add_custom_target(custom-commands ALL
	COMMAND ${CMAKE_COMMAND} -E echo "Running custom commands..."
)

# Copy assets to target dir
add_custom_command(TARGET custom-commands
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets/ $<TARGET_FILE_DIR:${TARGET_NAME}>/assets/
)

# Create the shader output directory
add_custom_command(TARGET custom-commands
	COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/shaders/
)

# Compile shaders to SPIR-V
foreach(FILE_PATH IN LISTS PROJECT_SHADERS)
	cmake_path(GET FILE_PATH FILENAME FILE_NAME)

	add_custom_command(TARGET custom-commands
		COMMAND glslc ${FILE_PATH} -o $<TARGET_FILE_DIR:${TARGET_NAME}>/shaders/${FILE_NAME}.spv
	)
endforeach()
