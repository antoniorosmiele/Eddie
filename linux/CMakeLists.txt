cmake_minimum_required(VERSION 3.12)
project(eddie-linux)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
find_package(Threads)
pkg_search_module(Coap REQUIRED IMPORTED_TARGET libcoap-3 libcoap-3-openssl)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0 gio-2.0)
pkg_search_module(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_compile_definitions(DEBUG)
endif()

add_subdirectory(communication)
add_subdirectory(intelligence)
add_subdirectory(virtualization)
add_subdirectory(hardware)
add_compile_definitions(DESCRIPTORS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/eddie")

if(NOT (SKIP_TESTS STREQUAL "true"))
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

# resource directory executable
add_executable(resource-directory examples/resource_directory.cpp ${SOURCES})
target_link_libraries(resource-directory PkgConfig::Coap ${CMAKE_THREAD_LIBS_INIT} eddie-communication)
set_property(TARGET resource-directory PROPERTY C_STANDARD 11)
install(TARGETS resource-directory RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# eddie endpoint executable
add_executable(eddie-endpoint examples/eddie_endpoint.cpp ${SOURCES})
target_link_libraries(eddie-endpoint PkgConfig::Coap ${CMAKE_THREAD_LIBS_INIT} eddie-communication)
target_include_directories(eddie-endpoint PUBLIC ${PROJECT_SOURCE_DIR}/include)
set_property(TARGET eddie-endpoint PROPERTY C_STANDARD 11)
install(TARGETS eddie-endpoint RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# virtualization server
add_executable(eddie-virt-server examples/virt_server.cpp ${SOURCES})
target_link_libraries(eddie-virt-server PkgConfig::GLIB ${CMAKE_THREAD_LIBS_INIT} eddie-communication eddie-virtualization eddie-hardware)
target_include_directories(eddie-virt-server PUBLIC ${PROJECT_SOURCE_DIR}/include)
set_property(TARGET eddie-virt-server PROPERTY C_STANDARD 11)
install(TARGETS eddie-virt-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(eddie-virt-client examples/virt_client.cpp ${SOURCES})
target_link_libraries(eddie-virt-client PkgConfig::GLIB ${CMAKE_THREAD_LIBS_INIT} eddie-virtualization)
set_property(TARGET eddie-virt-client PROPERTY C_STANDARD 11)
install(TARGETS eddie-virt-client RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(hw examples/hw.cpp ${SOURCES})
target_link_libraries(hw ${CMAKE_THREAD_LIBS_INIT} eddie-virtualization eddie-hardware jsoncpp eddie-common eddie-intelligence)
target_include_directories(hw PUBLIC ${PROJECT_SOURCE_DIR}/include)
set_property(TARGET hw PROPERTY C_STANDARD 11)
install(TARGETS hw RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(app examples/app.cpp ${SOURCES})
target_link_libraries(app ${CMAKE_THREAD_LIBS_INIT} eddie-virtualization)
target_include_directories(app PUBLIC ${PROJECT_SOURCE_DIR}/include)
set_property(TARGET app PROPERTY C_STANDARD 11)
install(TARGETS app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
