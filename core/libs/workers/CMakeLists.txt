add_library(workers STATIC 
	src/beta.cpp 
	src/correlation.cpp 
	src/ewma.cpp 
	src/zscore.cpp
)

target_include_directories(workers 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include 
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_directories(workers
	PUBLIC
		queue
)