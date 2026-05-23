add_executable(trading_pipeline
	main.cpp
)

target_link_libraries(trading_pipeline
	PRIVATE
		config 
		transport 
		signal
)

set_target_properties(trading_pipeline PROPERTIES 
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)