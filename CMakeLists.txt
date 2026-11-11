cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CATALIST_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)
string(STRIP "${CATALIST_VERSION}" CATALIST_VERSION)
project(catalist VERSION ${CATALIST_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# clangd reads build/compile_commands.json (see .clangd); without it every
# project include shows up as an error in the editor.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
  )
  FetchContent_MakeAvailable(ftxui)
endif()

set(CATALIST_WARNING_FLAGS -Wall -Wextra -Wpedantic)

set(CATALIST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/version.hpp.in "${CATALIST_GENERATED_DIR}/version.hpp" @ONLY)
configure_file(doc/catalist.1.in "${CMAKE_CURRENT_BINARY_DIR}/doc/catalist.1" @ONLY)

# Everything but main(), so the tests link the same code the program runs.
add_library(catalistLib STATIC
  src/catalogue.cpp
  src/cli.cpp
  src/date.cpp
  src/entry.cpp
  src/jikan.cpp
  src/json.cpp
  src/malXml.cpp
  src/system.cpp
  src/tui/app.cpp
  src/tui/catalogTab.cpp
  src/tui/catalogView.cpp
  src/tui/style.cpp
)
target_include_directories(catalistLib PUBLIC src "${CATALIST_GENERATED_DIR}")
target_link_libraries(catalistLib PUBLIC
  ftxui::screen
  ftxui::dom
  ftxui::component
)
target_compile_options(catalistLib PRIVATE ${CATALIST_WARNING_FLAGS})

add_executable(catalist src/main.cpp)
target_link_libraries(catalist PRIVATE catalistLib)
target_compile_options(catalist PRIVATE ${CATALIST_WARNING_FLAGS})

install(TARGETS catalist DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/doc/catalist.1"
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

enable_testing()

find_package(doctest CONFIG QUIET)
if(doctest_FOUND)
  include(doctest)
else()
  include(FetchContent)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
  )
  FetchContent_MakeAvailable(doctest)
  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
endif()

add_executable(catalistTests
  tests/catalogueTest.cpp
  tests/cliTest.cpp
  tests/dateTest.cpp
  tests/entryTest.cpp
  tests/jikanTest.cpp
  tests/jsonTest.cpp
  tests/malXmlTest.cpp
  tests/resultTest.cpp
  tests/support/sampleLibrary.cpp
  tests/support/testSupport.cpp
  tests/systemTest.cpp
  tests/testMain.cpp
  tests/tui/appShellTest.cpp
  tests/tui/catalogTabTest.cpp
  tests/tui/styleTest.cpp
  tests/tui/tuiHarness.cpp
  tests/versionTest.cpp
)
target_include_directories(catalistTests PRIVATE tests)
target_compile_definitions(catalistTests PRIVATE
  CATALIST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)
target_link_libraries(catalistTests PRIVATE doctest::doctest catalistLib)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)
