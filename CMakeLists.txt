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
  src/core/text.cpp
  # The lists in SQLite, the undo history, and the MyAnimeList XML format.
  src/storage/accounts.cpp
  src/storage/catalogue.cpp
  src/storage/csvCodec.cpp
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
  src/cli/tracking.cpp
  src/cli/transfer.cpp
  # Everything that touches the world outside the program.
  src/system.cpp
)
target_include_directories(catalistLib PUBLIC src "${CATALIST_GENERATED_DIR}")
# runProgram feeds a program's input from a second thread.
find_package(Threads REQUIRED)
target_link_libraries(catalistLib PUBLIC fmt::fmt ${CATALIST_SQLITE} Threads::Threads)
target_compile_options(catalistLib PRIVATE ${CATALIST_WARNING_FLAGS})

add_executable(catalist src/main.cpp)
target_link_libraries(catalist PRIVATE catalistLib)
target_compile_options(catalist PRIVATE ${CATALIST_WARNING_FLAGS})

# `cl` is catalist under a shorter name: a link next to the program, which
# then calls itself `cl` in its messages.
add_custom_command(TARGET catalist POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink catalist "$<TARGET_FILE_DIR:catalist>/cl"
  VERBATIM
)

# The program writes its manual page and its completion scripts from its
# own table of commands (see cmake/shellSupport.cmake).
set(CATALIST_SHARE_DIR "${CMAKE_CURRENT_BINARY_DIR}/share")
set(CATALIST_SHELL_FILES
  "${CATALIST_SHARE_DIR}/man/man1/catalist.1"
  "${CATALIST_SHARE_DIR}/zsh/_catalist"
  "${CATALIST_SHARE_DIR}/fish/catalist.fish"
  "${CATALIST_SHARE_DIR}/fish/cl.fish"
  "${CATALIST_SHARE_DIR}/bash/catalist"
  "${CATALIST_SHARE_DIR}/bash/cl"
)
add_custom_command(
  OUTPUT ${CATALIST_SHELL_FILES}
  COMMAND ${CMAKE_COMMAND}
    "-DPROGRAM=$<TARGET_FILE:catalist>"
    "-DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/docs/catalist.1.in"
    "-DOUT_DIR=${CATALIST_SHARE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/shellSupport.cmake"
  DEPENDS catalist docs/catalist.1.in cmake/shellSupport.cmake
  COMMENT "Writing the manual page and the completions"
  VERBATIM
)
add_custom_target(catalistShellSupport ALL DEPENDS ${CATALIST_SHELL_FILES})

# fish reads completions from the directory its pkg-config file names, when
# that is inside this install; anywhere else it would not be ours to write.
set(fishDir "${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d")
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_get_variable(fishPkgDir fish completionsdir)
  if(fishPkgDir)
    cmake_path(IS_PREFIX CMAKE_INSTALL_PREFIX "${fishPkgDir}" NORMALIZE fishDirIsOurs)
    if(fishDirIsOurs)
      set(fishDir "${fishPkgDir}")
    endif()
  endif()
endif()
set(CATALIST_FISH_COMPLETIONS_DIR "${fishDir}" CACHE PATH
  "Where the fish completions are installed")

install(TARGETS catalist DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "$<TARGET_FILE_DIR:catalist>/cl" DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${CATALIST_SHARE_DIR}/man/man1/catalist.1"
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES "${CATALIST_SHARE_DIR}/zsh/_catalist"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions)
install(FILES "${CATALIST_SHARE_DIR}/fish/catalist.fish" "${CATALIST_SHARE_DIR}/fish/cl.fish"
  DESTINATION ${CATALIST_FISH_COMPLETIONS_DIR})
install(FILES "${CATALIST_SHARE_DIR}/bash/catalist" "${CATALIST_SHARE_DIR}/bash/cl"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions)

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
  tests/support/sampleLibrary.cpp
  tests/support/testSupport.cpp
  tests/core/dateTest.cpp
  tests/core/entryTest.cpp
  tests/core/resultTest.cpp
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
  tests/web/malTest.cpp
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
  tests/cli/trackingTest.cpp
  tests/cli/transferTest.cpp
  tests/systemTest.cpp
)
target_include_directories(catalistTests PRIVATE tests)
target_compile_definitions(catalistTests PRIVATE
  CATALIST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)
target_link_libraries(catalistTests PRIVATE doctest::doctest catalistLib)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)

# The command line from the shells, as scripts use it; each shell found
# runs it. A stub curl stands in for AniList.
set(smokeScript "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/smoke.sh")
find_program(CATALIST_DASH dash)
find_program(CATALIST_BASH bash)
find_program(CATALIST_ZSH zsh)
find_program(CATALIST_FISH fish)
if(CATALIST_DASH)
  add_test(NAME smoke.dash COMMAND "${CATALIST_DASH}" "${smokeScript}" $<TARGET_FILE:catalist>)
endif()
if(CATALIST_BASH)
  add_test(NAME smoke.bash
    COMMAND "${CATALIST_BASH}" --posix "${smokeScript}" $<TARGET_FILE:catalist>)
  add_test(NAME completion.bash COMMAND "${CATALIST_BASH}" -n "${CATALIST_SHARE_DIR}/bash/catalist")
  # The script completes like a person typing, against a stub program.
  add_test(NAME completion.bash.words
    COMMAND "${CATALIST_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/completion.bash"
      "${CATALIST_SHARE_DIR}/bash/catalist" $<TARGET_FILE:catalist>)
  set_tests_properties(completion.bash.words PROPERTIES SKIP_RETURN_CODE 77)
endif()
if(CATALIST_ZSH)
  add_test(NAME smoke.zsh COMMAND "${CATALIST_ZSH}" -f "${smokeScript}" $<TARGET_FILE:catalist>)
  add_test(NAME completion.zsh COMMAND "${CATALIST_ZSH}" -n "${CATALIST_SHARE_DIR}/zsh/_catalist")
endif()
if(CATALIST_FISH)
  add_test(NAME smoke.fish
    COMMAND "${CATALIST_FISH}" --no-config "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke/smoke.fish"
      $<TARGET_FILE:catalist>)
  add_test(NAME completion.fish
    COMMAND "${CATALIST_FISH}" --no-config -n "${CATALIST_SHARE_DIR}/fish/catalist.fish"
      "${CATALIST_SHARE_DIR}/fish/cl.fish")
endif()
