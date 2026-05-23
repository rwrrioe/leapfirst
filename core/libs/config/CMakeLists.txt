add_library(config STATIC 
	src/config.cpp
)

target_include_directories(config 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src 
)

target_link_libraries(config 
	PRIVATE nlohmann_json::nlohmann_json
)

target_compile_definitions(config 
	PRIVATE PROJECT_VERSION="${PROJECT_VERSION}"
)