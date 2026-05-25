add_library(config STATIC 
	src/config.cpp
)

target_include_directories(config 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src 
)

target_compile_definitions(config 
	PRIVATE PROJECT_VERSION="${PROJECT_VERSION}"
)