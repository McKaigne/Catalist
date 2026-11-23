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

# The ids the sites give a registered program, for their APIs and logins:
# MyAnimeList's (https://myanimelist.net/apiconfig, type "other") even for
# public data; AniList's (https://anilist.co/settings/developer, redirect URL
# https://anilist.co/api/v2/oauth/pin) and Kitsu's to log in. The same names
# in the environment override them at run time.
set(CATALIST_ANILIST_CLIENT_ID "" CACHE STRING "The client id AniList gave Catalist")
set(CATALIST_MAL_CLIENT_ID "" CACHE STRING "The client id MyAnimeList gave Catalist")
set(CATALIST_KITSU_CLIENT_ID "" CACHE STRING "The client id Kitsu gave Catalist")
set(CATALIST_KITSU_CLIENT_SECRET "" CACHE STRING "The client secret Kitsu gave Catalist")
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
  src/storage/accounts.cpp
  src/storage/catalogue.cpp
  src/storage/csv_codec.cpp
  src/storage/history.cpp
  src/storage/journal.cpp
  src/storage/mal_xml.cpp
  src/storage/store.cpp
  # The sites behind one tracker interface, and the JSON they answer in.
  src/web/anilist.cpp
  src/web/http.cpp
  src/web/json.cpp
  src/web/kitsu.cpp
  src/web/mal.cpp
  src/web/sites.cpp
  src/web/tracker.cpp
  # The command line.
  src/cli/accounts.cpp
  src/cli/args.cpp
  src/cli/cli.cpp
  src/cli/common.cpp
  src/cli/completion.cpp
  src/cli/history.cpp
  src/cli/listing.cpp
  src/cli/online.cpp
  src/cli/output.cpp
  src/cli/refs.cpp
  src/cli/setup.cpp
  src/cli/sync.cpp
  src/cli/track.cpp
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
  tests/storage/accounts_test.cpp
  tests/storage/catalogue_test.cpp
  tests/storage/csv_codec_test.cpp
  tests/storage/journal_test.cpp
  tests/storage/mal_xml_test.cpp
  tests/web/anilist_test.cpp
  tests/web/json_test.cpp
  tests/web/kitsu_test.cpp
  tests/web/login_test.cpp
  tests/web/pull_test.cpp
  tests/web/push_test.cpp
  tests/web/mal_test.cpp
  tests/web/entry_of_test.cpp
  tests/cli/accounts_test.cpp
  tests/cli/args_test.cpp
  tests/cli/cli_test.cpp
  tests/cli/completion_test.cpp
  tests/cli/history_test.cpp
  tests/cli/listing_test.cpp
  tests/cli/online_test.cpp
  tests/cli/output_test.cpp
  tests/cli/refs_test.cpp
  tests/cli/setup_test.cpp
  tests/cli/sync_test.cpp
  tests/cli/track_test.cpp
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
