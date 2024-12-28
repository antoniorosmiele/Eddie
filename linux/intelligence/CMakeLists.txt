cmake_minimum_required(VERSION 3.12)
project(eddie-intelligence)

find_package(PkgConfig REQUIRED)
find_package(Threads)
pkg_search_module(jsoncpp REQUIRED IMPORTED_TARGET jsoncpp)

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_compile_definitions(DEBUG)
endif ()

add_compile_definitions(DESCRIPTORS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/eddie")
add_library(${PROJECT_NAME} src/engine.cpp
                            src/DFS.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} eddie-communication PkgConfig::jsoncpp)