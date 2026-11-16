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
    GIT_TAG v7.0.3
  )
  FetchContent_MakeAvailable(ftxui)
endif()

find_package(fmt QUIET)
if(NOT fmt_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 12.2.0
  )
  FetchContent_MakeAvailable(fmt)
endif()

# The lists are kept in an SQLite file. Without a system library, SQLite's
# single-file source is downloaded and built in.
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
  include(FetchContent)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip
    URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032
  )
  FetchContent_MakeAvailable(sqlite3)
  enable_language(C)
  add_library(catalistSqlite STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
  target_include_directories(catalistSqlite PUBLIC "${sqlite3_SOURCE_DIR}")
  target_compile_definitions(catalistSqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION)
  # Not this project's code: its warnings are not ours to fix.
  target_compile_options(catalistSqlite PRIVATE -w)
  find_package(Threads REQUIRED)
  target_link_libraries(catalistSqlite PUBLIC Threads::Threads)
  set(CATALIST_SQLITE catalistSqlite)
elseif(TARGET SQLite3::SQLite3)
  set(CATALIST_SQLITE SQLite3::SQLite3)
else()
  # The target's name before CMake 4.3.
  set(CATALIST_SQLITE SQLite::SQLite3)
endif()

set(CATALIST_WARNING_FLAGS -Wall -Wextra -Wpedantic)

set(CATALIST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/version.hpp.in "${CATALIST_GENERATED_DIR}/version.hpp" @ONLY)
configure_file(doc/catalist.1.in "${CMAKE_CURRENT_BINARY_DIR}/doc/catalist.1" @ONLY)

# Everything but main(), so the tests link the same code the program runs.
add_library(catalistLib STATIC
  src/anilist.cpp
  src/args.cpp
  src/catalogue.cpp
  src/cli.cpp
  src/cliLegacy.cpp
  src/date.cpp
  src/entry.cpp
  src/journal.cpp
  src/json.cpp
  src/malXml.cpp
  src/output.cpp
  src/refs.cpp
  src/store.cpp
  src/system.cpp
  src/text.cpp
  src/tui/app.cpp
  src/tui/catalogTab.cpp
  src/tui/catalogView.cpp
  src/tui/style.cpp
  src/updater.cpp
)
target_include_directories(catalistLib PUBLIC src "${CATALIST_GENERATED_DIR}")
target_link_libraries(catalistLib PUBLIC
  fmt::fmt
  ${CATALIST_SQLITE}
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
  tests/anilistTest.cpp
  tests/argsTest.cpp
  tests/catalogueTest.cpp
  tests/cliTest.cpp
  tests/dateTest.cpp
  tests/entryTest.cpp
  tests/journalTest.cpp
  tests/jsonTest.cpp
  tests/malXmlTest.cpp
  tests/outputTest.cpp
  tests/refsTest.cpp
  tests/resultTest.cpp
  tests/support/sampleLibrary.cpp
  tests/support/testSupport.cpp
  tests/systemTest.cpp
  tests/testMain.cpp
  tests/tui/appShellTest.cpp
  tests/tui/catalogTabTest.cpp
  tests/tui/styleTest.cpp
  tests/tui/tuiHarness.cpp
  tests/updaterTest.cpp
  tests/versionTest.cpp
)
target_include_directories(catalistTests PRIVATE tests)
target_compile_definitions(catalistTests PRIVATE
  CATALIST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)
target_link_libraries(catalistTests PRIVATE doctest::doctest catalistLib)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)
