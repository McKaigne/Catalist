cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CATALIST_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)
string(STRIP "${CATALIST_VERSION}" CATALIST_VERSION)
project(catalist VERSION ${CATALIST_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# clangd reads build/compile_commands.json (see .clangd); without it every
# project include shows up as an error in the editor.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
  src/storage/csvCodec.cpp
  src/storage/history.cpp
  src/storage/journal.cpp
  src/storage/malXml.cpp
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

# The program writes its manual page and its completion scripts from its
# own table of commands (see cmake/shellSupport.cmake).
set(CATALIST_SHARE_DIR "${CMAKE_CURRENT_BINARY_DIR}/share")
set(CATALIST_SHELL_FILES
  "${CATALIST_SHARE_DIR}/man/man1/catalist.1"
  "${CATALIST_SHARE_DIR}/zsh/_catalist"
  "${CATALIST_SHARE_DIR}/fish/catalist.fish"
)
add_custom_command(
  OUTPUT ${CATALIST_SHELL_FILES}
  COMMAND ${CMAKE_COMMAND}
    "-DPROGRAM=$<TARGET_FILE:catalist>"
    "-DVERSION=${PROJECT_VERSION}"
    "-DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/docs/catalist.1.in"
    "-DOUT_DIR=${CATALIST_SHARE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/shellSupport.cmake"
  DEPENDS catalist docs/catalist.1.in cmake/shellSupport.cmake
  COMMENT "Writing the manual page and the completions"
  VERBATIM
)
add_custom_target(catalistShellSupport ALL DEPENDS ${CATALIST_SHELL_FILES})

install(TARGETS catalist DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${CATALIST_SHARE_DIR}/man/man1/catalist.1"
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES "${CATALIST_SHARE_DIR}/zsh/_catalist"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions)
install(FILES "${CATALIST_SHARE_DIR}/fish/catalist.fish"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)

option(BUILD_TESTING "Build the tests (needs doctest)" ON)
if(NOT BUILD_TESTING)
  return()
endif()
enable_testing()

find_package(doctest CONFIG REQUIRED)
include(doctest)

add_executable(catalistTests
  tests/testMain.cpp
  tests/support/sampleLibrary.cpp
  tests/support/testSupport.cpp
  tests/core/dateTest.cpp
  tests/core/entryTest.cpp
  tests/core/listsTest.cpp
  tests/core/textTest.cpp
  tests/core/versionTest.cpp
  tests/storage/accountsTest.cpp
  tests/storage/catalogueTest.cpp
  tests/storage/csvCodecTest.cpp
  tests/storage/journalTest.cpp
  tests/storage/malXmlTest.cpp
  tests/web/anilistTest.cpp
  tests/web/jsonTest.cpp
  tests/web/kitsuTest.cpp
  tests/web/loginTest.cpp
  tests/web/pullTest.cpp
  tests/web/pushTest.cpp
  tests/web/malTest.cpp
  tests/web/entryOfTest.cpp
  tests/cli/accountsTest.cpp
  tests/cli/argsTest.cpp
  tests/cli/cliTest.cpp
  tests/cli/completionTest.cpp
  tests/cli/historyTest.cpp
  tests/cli/listingTest.cpp
  tests/cli/onlineTest.cpp
  tests/cli/outputTest.cpp
  tests/cli/refsTest.cpp
  tests/cli/setupTest.cpp
  tests/cli/syncTest.cpp
  tests/cli/trackTest.cpp
  tests/cli/trackingTest.cpp
  tests/cli/transferTest.cpp
  tests/systemTest.cpp
)
target_include_directories(catalistTests PRIVATE tests)
target_link_libraries(catalistTests PRIVATE doctest::doctest catalistLib)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)

# The command line from a POSIX shell, as scripts use it; a stub curl
# stands in for the sites. The completion scripts must parse.
add_test(NAME smoke COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/smoke.sh"
  $<TARGET_FILE:catalist>)
find_program(CATALIST_ZSH zsh)
find_program(CATALIST_FISH fish)
if(CATALIST_ZSH)
  add_test(NAME completion.zsh COMMAND "${CATALIST_ZSH}" -n "${CATALIST_SHARE_DIR}/zsh/_catalist")
endif()
if(CATALIST_FISH)
  add_test(NAME completion.fish
    COMMAND "${CATALIST_FISH}" --no-config -n "${CATALIST_SHARE_DIR}/fish/catalist.fish")
endif()
