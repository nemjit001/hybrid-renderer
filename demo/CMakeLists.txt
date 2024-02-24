project("hybrid-renderer-demo" VERSION 1.0.0)
set(TARGET_NAME hrdemo)

file(GLOB PROJECT_HEADERS "include/*.h" "include/**/*.h")
file(GLOB PROJECT_SOURCES "src/*.cpp" "src/**/*.cpp")
file(GLOB PROJECT_SHADERS "shaders/*.vert" "shaders/*.frag" "shaders/*.comp" "shaders/*.rgen" "shaders/*.rmiss" "shaders/*.rchit" "shaders/*.rahit")

# Add dependencies
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory("deps/glfw/")

add_subdirectory("deps/tinyobjloader/")

file(GLOB IMGUI_HEADERS "deps/imgui/*.h")
file(GLOB IMGUI_SOURCES "deps/imgui/*.cpp")
set(IMGUI_HEADERS ${IMGUI_HEADERS} "deps/imgui/backends/imgui_impl_glfw.h" "deps/imgui/backends/imgui_impl_vulkan.h")
set(IMGUI_SOURCES ${IMGUI_SOURCES} "deps/imgui/backends/imgui_impl_glfw.cpp" "deps/imgui/backends/imgui_impl_vulkan.cpp")

add_executable(${TARGET_NAME}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
	${PROJECT_SHADERS}

	# imgui dependency
	${IMGUI_HEADERS}
	${IMGUI_SOURCES}
)

target_link_libraries(${TARGET_NAME} PRIVATE
	glfw ${GLFW_LIBRARIES}
	tinyobjloader
	hybridrenderer
)

target_include_directories(${TARGET_NAME} PRIVATE
	"include/"
	"deps/glfw/include/"
	"deps/tinyobjloader/"
	"deps/imgui/"
	"deps/imgui/backends"
	"../hybrid-renderer-lib/include/"
)

add_custom_target(custom-commands-${TARGET_NAME} ALL
	COMMAND ${CMAKE_COMMAND} -E echo "Running custom commands..."
)

# Copy assets to target dir
add_custom_command(TARGET custom-commands-${TARGET_NAME}
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets/ $<TARGET_FILE_DIR:${TARGET_NAME}>/assets/
)

# Create shader output directory
add_custom_command(TARGET custom-commands-${TARGET_NAME}
	COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/shaders/
)

# Compile shaders & deposit in shader directory
foreach(SHADER_PATH IN LISTS PROJECT_SHADERS)
	cmake_path(GET SHADER_PATH FILENAME SHADER_FILE)

	add_custom_command(TARGET custom-commands-${TARGET_NAME}
		COMMAND glslc ${SHADER_PATH} -o $<TARGET_FILE_DIR:${TARGET_NAME}>/shaders/${SHADER_FILE}.spv
	)
endforeach()
