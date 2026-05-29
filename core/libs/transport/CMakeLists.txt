add_library(transport STATIC
    src/websocket_client.cpp
    src/kafka.cpp
)

target_include_directories(transport
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/signal/include
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/transport/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(transport
    PUBLIC
        signal
        workers
    PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        RdKafka::rdkafka++
        simdjson::simdjson
)

if(WIN32)
    target_link_libraries(transport PRIVATE ws2_32 mswsock)
endif()

if(MSVC)
    target_compile_options(transport PRIVATE /bigobj)
endif()
