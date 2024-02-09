project("hybrid-renderer-lib" VERSION 1.0.0)
set(LIB_NAME hybridrenderer)

file(GLOB PROJECT_HEADERS "include/*.h" "include/**/*.h")
file(GLOB PROJECT_SOURCES "src/*.cpp" "src/**/*.cpp")

add_library(${LIB_NAME}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
)

target_include_directories(${LIB_NAME} PUBLIC
	"include/"
)
