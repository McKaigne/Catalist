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

set(CATALIST_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(
  include/catalist/core/version.hpp.in
  "${CATALIST_GENERATED_INCLUDE_DIR}/catalist/core/version.hpp"
  @ONLY
)
configure_file(doc/catalist.1.in "${CMAKE_CURRENT_BINARY_DIR}/doc/catalist.1" @ONLY)

# Everything but the terminal UI. Must stay free of FTXUI so its tests can
# be built and run without one.
add_library(catalistCore STATIC
  src/cli.cpp
  src/core/clock.cpp
  src/core/date.cpp
  src/core/json.cpp
  src/core/shell.cpp
  src/core/url.cpp
  src/model/entry.cpp
  src/model/entryOps.cpp
  src/net/jikan.cpp
  src/platform/fileSystem.cpp
  src/platform/httpClient.cpp
  src/storage/csvCodec.cpp
  src/storage/database.cpp
  src/storage/malXml.cpp
)
target_include_directories(catalistCore PUBLIC
  include
  "${CATALIST_GENERATED_INCLUDE_DIR}"
)
target_compile_options(catalistCore PRIVATE ${CATALIST_WARNING_FLAGS})

add_library(catalistTui STATIC
  src/tui/app.cpp
  src/tui/keys.cpp
  src/tui/tabs/catalogTab.cpp
  src/tui/tabs/statsTab.cpp
  src/tui/theme/statusStyle.cpp
  src/tui/widgets/card.cpp
  src/tui/widgets/marquee.cpp
  src/tui/widgets/pill.cpp
)
target_link_libraries(catalistTui PUBLIC
  catalistCore
  ftxui::screen
  ftxui::dom
  ftxui::component
)
target_compile_options(catalistTui PRIVATE ${CATALIST_WARNING_FLAGS})

add_executable(catalist src/main.cpp)
target_link_libraries(catalist PRIVATE catalistCore catalistTui)
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
  tests/cliTest.cpp
  tests/csvCodecTest.cpp
  tests/databaseTest.cpp
  tests/dateTest.cpp
  tests/entryOpsTest.cpp
  tests/entryTest.cpp
  tests/fileSystemTest.cpp
  tests/jikanTest.cpp
  tests/jsonTest.cpp
  tests/malXmlTest.cpp
  tests/resultTest.cpp
  tests/shellTest.cpp
  tests/support/sampleLibrary.cpp
  tests/support/testSupport.cpp
  tests/testMain.cpp
  tests/urlTest.cpp
  tests/versionTest.cpp
)

target_include_directories(catalistTests PRIVATE tests)
target_compile_definitions(catalistTests PRIVATE
  CATALIST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)
target_link_libraries(catalistTests PRIVATE
  doctest::doctest
  catalistCore
)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)

# Tab and widget tests need FTXUI, so they live in their own binary;
# catalistTests stays free of it.
add_executable(catalistTuiTests
  tests/testMain.cpp
  tests/support/sampleLibrary.cpp
  tests/support/testSupport.cpp
  tests/tui/appShellTest.cpp
  tests/tui/catalogTabTest.cpp
  tests/tui/statsTabTest.cpp
  tests/tui/tuiHarness.cpp
  tests/tui/widgetsTest.cpp
)
target_include_directories(catalistTuiTests PRIVATE tests)
target_compile_definitions(catalistTuiTests PRIVATE
  CATALIST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)
target_link_libraries(catalistTuiTests PRIVATE
  doctest::doctest
  catalistCore
  catalistTui
)
target_compile_options(catalistTuiTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTuiTests)
