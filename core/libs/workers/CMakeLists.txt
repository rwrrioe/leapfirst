add_library(workers STATIC 
	src/beta.cpp 
	src/correlation.cpp 
	src/ewma.cpp 
	src/zscore.cpp
)

target_include_directories(workers 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include 
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/signal/include
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/transport/include  
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(workers
	PUBLIC
		queue
		simdjson::simdjson
)