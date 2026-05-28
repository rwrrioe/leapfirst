cmake_minimum_required(VERSION 3.20)
project(leapfirst_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

find_package(RdKafka CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(boost_system CONFIG REQUIRED)
find_package(boost_asio   CONFIG REQUIRED)
find_package(boost_beast  CONFIG REQUIRED)
find_package(simdjson REQUIRED)

add_subdirectory(libs/config)
add_subdirectory(libs/queue)
add_subdirectory(libs/workers)
add_subdirectory(libs/signal)
add_subdirectory(libs/transport)
add_subdirectory(src)
