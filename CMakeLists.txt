cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CATALIST_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)
string(STRIP "${CATALIST_VERSION}" CATALIST_VERSION)
project(catalist VERSION ${CATALIST_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set(CATALIST_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(
  include/catalist/core/version.hpp.in
  "${CATALIST_GENERATED_INCLUDE_DIR}/catalist/core/version.hpp"
  @ONLY
)
configure_file(doc/catalist.1.in "${CMAKE_CURRENT_BINARY_DIR}/doc/catalist.1" @ONLY)

include_directories(include "${CATALIST_GENERATED_INCLUDE_DIR}")

set(CATALIST_WARNING_FLAGS -Wall -Wextra -Wpedantic)

set(CATALIST_SOURCES
  src/core/clock.cpp
  src/core/date.cpp
  src/core/jsonLite.cpp
  src/core/logger.cpp
  src/core/shell.cpp
  src/core/url.cpp
  src/platform/fileSystem.cpp
  src/platform/httpClient.cpp
  src/platform/processLauncher.cpp
  src/kdl.cpp
  src/entry.cpp
  src/config.cpp
  src/database.cpp
  src/malParser.cpp
  src/sync.cpp
  src/cli.cpp
  src/remoteSearch.cpp
  src/themeRegistry.cpp
  src/keybindManager.cpp
  src/commandPalette.cpp
  src/provider.cpp
)

add_executable(catalist
  ${CATALIST_SOURCES}
  src/tui.cpp
  src/main.cpp
)

target_link_libraries(catalist PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

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
  tests/testMain.cpp
  tests/support/testSupport.cpp
  tests/support/sampleLibrary.cpp
  tests/cliGoldenTest.cpp
  tests/cliSetTest.cpp
  tests/cliTest.cpp
  tests/configTest.cpp
  tests/databaseTest.cpp
  tests/entryTest.cpp
  tests/fileSystemTest.cpp
  tests/jsonLiteTest.cpp
  tests/kdlTest.cpp
  tests/keybindManagerTest.cpp
  tests/dateTest.cpp
  tests/loggerTest.cpp
  tests/malAuthTest.cpp
  tests/malParserTest.cpp
  tests/providerTest.cpp
  tests/remoteSearchTest.cpp
  tests/resultTest.cpp
  tests/shellTest.cpp
  tests/urlTest.cpp
  tests/versionTest.cpp
  ${CATALIST_SOURCES}
)

target_include_directories(catalistTests PRIVATE tests)
target_compile_definitions(catalistTests PRIVATE
  CATALIST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)

target_link_libraries(catalistTests PRIVATE
  doctest::doctest
  ftxui::screen
  ftxui::dom
)

target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)
