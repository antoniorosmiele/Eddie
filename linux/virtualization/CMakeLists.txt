cmake_minimum_required(VERSION 3.12)
project(eddie-virtualization)

find_package(PkgConfig REQUIRED)
find_package(Threads)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0 gio-2.0)
pkg_check_modules(jsoncpp REQUIRED IMPORTED_TARGET jsoncpp)

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_compile_definitions(DEBUG)
endif()

add_library(${PROJECT_NAME} src/VirtualizationReceiver.cpp src/VirtualizationSender.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include ${GLIB_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PkgConfig::Coap ${CMAKE_THREAD_LIBS_INIT} ${GLIB_LIBRARIES} eddie-communication eddie-intelligence PkgConfig::jsoncpp)