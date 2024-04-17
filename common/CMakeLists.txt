cmake_minimum_required(VERSION 3.12)
project(eddie-common)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_compile_definitions(DEBUG)
endif()

add_library(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/src/LinkFormat.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
