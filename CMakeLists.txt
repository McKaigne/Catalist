cmake_minimum_required(VERSION 3.20)
project(catalist VERSION 0.2.0 LANGUAGES CXX)

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

include_directories(include)

set(CATALIST_SOURCES
  src/core/date.cpp
  src/core/logger.cpp
  src/platform/httpClient.cpp
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

install(TARGETS catalist DESTINATION bin)

enable_testing()

find_package(doctest CONFIG QUIET)
if(doctest_FOUND)
  include(doctest)
else()
  include(FetchContent)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
  )
  FetchContent_MakeAvailable(doctest)
  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
endif()

add_executable(catalistTests
  tests/testMain.cpp
  tests/support/testSupport.cpp
  tests/cliTest.cpp
  tests/databaseTest.cpp
  tests/dateTest.cpp
  tests/loggerTest.cpp
  tests/malParserTest.cpp
  tests/providerTest.cpp
  tests/resultTest.cpp
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

doctest_discover_tests(catalistTests)
