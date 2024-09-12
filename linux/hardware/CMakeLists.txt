cmake_minimum_required(VERSION 3.12)
project(eddie-hardware)

if (CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    add_compile_definitions(DEBUG)
endif()

pkg_search_module(jsoncpp REQUIRED IMPORTED_TARGET jsoncpp)

add_compile_definitions(DESCRIPTORS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/eddie")
add_library(${PROJECT_NAME}
        src/EddieBlinkingLamp.cpp
        src/GPIOWriter.cpp
        src/hardware_utils.cpp
        src/EddieTemperature.cpp
        src/OneWireReader.cpp
        src/EddieVirtualAlarm.cpp
        src/EddieActuatorsWater.cpp
        src/EddieAllarmNoisy.cpp
        src/EddieAllarmSilent.cpp
        src/EddieAudioSpeaker.cpp
        src/EddieBulb.cpp
        src/EddieCalculatorCPU.cpp
        src/EddieCamera.cpp
        src/EddieDataset.cpp
        src/EddieDoor.cpp
        src/EddieFacialRecognizer.cpp
        src/EddieMattressCover.cpp
        src/EddieMicrophone.cpp
        src/EddieMonitorVideo.cpp
        src/EddieSensorBracelet.cpp
        src/EddieService.cpp
        src/EddieTemperatureSensor.cpp
        src/EddieTent.cpp
        src/EddieVibrationAllarm.cpp
        src/FakeResource.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} eddie-common eddie-communication PkgConfig::jsoncpp)