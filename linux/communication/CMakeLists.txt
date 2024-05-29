cmake_minimum_required(VERSION 3.12)
project(eddie-communication)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

find_package(PkgConfig REQUIRED)
find_package(Threads)
pkg_search_module(Coap REQUIRED IMPORTED_TARGET libcoap-3 libcoap-3-openssl)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_compile_definitions(DEBUG)
endif()

add_subdirectory(../../common common)

add_library(
    ${PROJECT_NAME} 
    src/CoapClient.cpp 
    src/CoapServer.cpp 
    src/ResourceDirectory.cpp 
    src/common.cpp
    src/EddieEndpoint.cpp
    src/MGM.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} PkgConfig::Coap eddie-common)
