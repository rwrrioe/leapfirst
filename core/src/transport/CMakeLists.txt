add_library(transport STATIC 
	src/ws_client.cpp
	src/kafka_producer.cpp
)

target_include_directories(transport
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include  
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(transport
	PUBLIC 
		signal
	PRIVATE
		Boost::system
        Boost::asio       
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        RdKafka::rdkafka++
        nlohmann_json::nlohmann_json
)

if (WIN32) 
	target_link_libraries(transport PRIVATE ws2_32 mswsock)
endif()