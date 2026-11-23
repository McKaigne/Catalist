cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CATALIST_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)
string(STRIP "${CATALIST_VERSION}" CATALIST_VERSION)
project(catalist VERSION ${CATALIST_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The lists are kept in an SQLite file.
find_package(SQLite3 REQUIRED)
if(TARGET SQLite3::SQLite3)
  set(CATALIST_SQLITE SQLite3::SQLite3)
else()
  # The target's name before CMake 4.3.
  set(CATALIST_SQLITE SQLite::SQLite3)
endif()

set(CATALIST_WARNING_FLAGS -Wall -Wextra -Wpedantic)

set(CATALIST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/core/version.hpp.in "${CATALIST_GENERATED_DIR}/core/version.hpp" @ONLY)

# Everything but main(), so the tests link the same code the program runs.
add_library(catalistLib STATIC
  # The title, its dates and rules, and UTF-8 text: no I/O.
  src/core/date.cpp
  src/core/entry.cpp
  src/core/lists.cpp
  src/core/text.cpp
  # The lists in SQLite, the undo history, and the MyAnimeList XML format.
  src/storage/catalogue.cpp
  src/storage/csv_codec.cpp
  src/storage/history.cpp
  src/storage/journal.cpp
  src/storage/mal_xml.cpp
  src/storage/store.cpp
  # AniList, and the JSON it answers in.
  src/web/anilist.cpp
  src/web/json.cpp
  # The command line.
  src/cli/args.cpp
  src/cli/cli.cpp
  src/cli/common.cpp
  src/cli/history.cpp
  src/cli/listing.cpp
  src/cli/online.cpp
  src/cli/output.cpp
  src/cli/refs.cpp
  src/cli/tracking.cpp
  src/cli/transfer.cpp
  # Everything that touches the world outside the program.
  src/system.cpp
)
target_include_directories(catalistLib PUBLIC src "${CATALIST_GENERATED_DIR}")
target_link_libraries(catalistLib PUBLIC ${CATALIST_SQLITE})
target_compile_options(catalistLib PRIVATE ${CATALIST_WARNING_FLAGS})

add_executable(catalist src/main.cpp)
target_link_libraries(catalist PRIVATE catalistLib)
target_compile_options(catalist PRIVATE ${CATALIST_WARNING_FLAGS})

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
  tests/storage/journal_test.cpp
  tests/storage/mal_xml_test.cpp
  tests/web/anilist_test.cpp
  tests/web/json_test.cpp
  tests/cli/args_test.cpp
  tests/cli/cli_test.cpp
  tests/cli/history_test.cpp
  tests/cli/listing_test.cpp
  tests/cli/online_test.cpp
  tests/cli/output_test.cpp
  tests/cli/refs_test.cpp
  tests/cli/tracking_test.cpp
  tests/cli/transfer_test.cpp
  tests/system_test.cpp
)
target_include_directories(catalistTests PRIVATE tests)
target_link_libraries(catalistTests PRIVATE doctest::doctest catalistLib)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)

# The command line from a POSIX shell, as scripts use it; a stub curl
# stands in for the site.
add_test(NAME smoke COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/smoke.sh"
  $<TARGET_FILE:catalist>)
