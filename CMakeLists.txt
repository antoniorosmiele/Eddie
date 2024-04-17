cmake_minimum_required(VERSION 3.12)
project(eddie)

include(GNUInstallDirs)

enable_testing()

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

add_subdirectory(linux)

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/eddie)

# indicating the documentation build as an option and set it to ON by default
option(BUILD_DOC "Build documentation" ON)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND AND BUILD_DOC)
  set(DOXYFILE ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)

  message("Doxygen build started")

  # the option ALL which allows to build the docs together with the application
  add_custom_target( doc_doxygen ALL
          COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          COMMENT "Generating API documentation with Doxygen"
          VERBATIM )
elseif (BUILD_DOC)
  message("If you want API documentation to be generated, Doxygen needs to be installed.")
endif (DOXYGEN_FOUND AND BUILD_DOC)
