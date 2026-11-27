cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CATALIST_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)
string(STRIP "${CATALIST_VERSION}" CATALIST_VERSION)
project(catalist VERSION ${CATALIST_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# clangd, the editor's language server, reads build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CATALIST_WARNING_FLAGS -Wall -Wextra -Wpedantic)

set(CATALIST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/core/version.hpp.in "${CATALIST_GENERATED_DIR}/core/version.hpp" @ONLY)

add_library(catalistLib STATIC
  src/core/date.cpp
  src/core/entry.cpp
  src/core/lists.cpp
  src/core/text.cpp
  src/storage/catalogue.cpp
  src/storage/csv_codec.cpp
  src/storage/mal_xml.cpp
  src/web/anilist.cpp
  src/web/json.cpp
  src/menu/add.cpp
  src/menu/common.cpp
  src/menu/find.cpp
  src/menu/input.cpp
  src/menu/list.cpp
  src/menu/menu.cpp
  src/menu/output.cpp
  src/menu/title.cpp
  src/menu/transfer.cpp
  src/system.cpp
)
target_include_directories(catalistLib PUBLIC src "${CATALIST_GENERATED_DIR}")
target_compile_options(catalistLib PRIVATE ${CATALIST_WARNING_FLAGS})

add_executable(catalist src/main.cpp)
target_link_libraries(catalist PRIVATE catalistLib)
target_compile_options(catalist PRIVATE ${CATALIST_WARNING_FLAGS})
target_compile_definitions(catalist PRIVATE CATALIST_HOME="${CMAKE_CURRENT_SOURCE_DIR}")

install(TARGETS catalist DESTINATION ${CMAKE_INSTALL_BINDIR})

option(BUILD_TESTING "Build the tests (needs doctest)" ON)
if(NOT BUILD_TESTING)
  return()
endif()
enable_testing()

find_package(doctest CONFIG REQUIRED)
include(doctest)

add_executable(catalistTests
  tests/test_main.cpp
  tests/support/sample_library.cpp
  tests/support/test_support.cpp
  tests/core/date_test.cpp
  tests/core/entry_test.cpp
  tests/core/lists_test.cpp
  tests/core/text_test.cpp
  tests/core/version_test.cpp
  tests/storage/catalogue_test.cpp
  tests/storage/csv_codec_test.cpp
  tests/storage/mal_xml_test.cpp
  tests/web/anilist_test.cpp
  tests/web/json_test.cpp
  tests/menu/find_test.cpp
  tests/menu/menu_test.cpp
  tests/menu/output_test.cpp
  tests/system_test.cpp
)
target_include_directories(catalistTests PRIVATE tests)
target_link_libraries(catalistTests PRIVATE doctest::doctest catalistLib)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)

add_test(NAME smoke COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/smoke.sh"
  $<TARGET_FILE:catalist>)
