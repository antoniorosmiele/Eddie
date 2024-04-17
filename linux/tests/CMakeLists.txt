add_executable(
  communication_tests
  communication_tests.cpp
)
add_executable(
  virtualization_tests
  virtualization_tests.cpp
)
target_link_libraries(
  communication_tests
  gtest
  gtest_main
  PkgConfig::Coap
  eddie-communication
)

target_link_libraries(
  virtualization_tests
  gtest
  gtest_main
  eddie-virtualization
)

include(GoogleTest)
gtest_discover_tests(communication_tests PROPERTIES FIXTURES_REQUIRED "eddie-virt-server")
gtest_discover_tests(virtualization_tests PROPERTIES FIXTURES_REQUIRED "eddie-virt-server")

add_test(NAME Fixture.Eddie_Server_Setup
  COMMAND ${CMAKE_SOURCE_DIR}/tests/fixture-start.sh "./linux/eddie-virt-server --exampleres --ip ::1")
set_tests_properties(Fixture.Eddie_Server_Setup
  PROPERTIES WORKING_DIRECTORY .
  FIXTURES_SETUP eddie-virt-server)

add_test(NAME Fixture.Eddie_Server_Teardown
  COMMAND ${CMAKE_SOURCE_DIR}/tests/fixture-stop.sh)
set_tests_properties(Fixture.Eddie_Server_Teardown
  PROPERTIES WORKING_DIRECTORY .
  FIXTURES_CLEANUP eddie-virt-server)
