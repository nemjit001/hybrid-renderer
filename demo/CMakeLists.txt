project("hybrid-renderer-demo" VERSION 1.0.0)
set(TARGET_NAME hrdemo)

file(GLOB PROJECT_HEADERS "include/*.h")
file(GLOB PROJECT_SOURCES "src/*.cpp")

add_executable(${TARGET_NAME}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
)

target_link_libraries(${TARGET_NAME} PRIVATE
	hybridrenderer
)

target_include_directories(${TARGET_NAME} PRIVATE
	"include/"
	"../hybrid-renderer-lib/include/"
)

add_custom_target(custom-commands ALL
	COMMAND ${CMAKE_COMMAND} -E echo
)

add_custom_command(TARGET custom-commands
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets/ $<TARGET_FILE_DIR:${TARGET_NAME}>/assets/
)
