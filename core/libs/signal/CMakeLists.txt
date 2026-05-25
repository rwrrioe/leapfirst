add_library(signal STATIC src/engine.cpp)
target_include_directories(signal 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(signal	
	PUBLIC queue
	PRIVATE workers
)