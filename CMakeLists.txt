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

set(CATALIST_WARNING_FLAGS -Wall -Wextra -Wpedantic)

set(CATALIST_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(
  include/catalist/core/version.hpp.in
  "${CATALIST_GENERATED_INCLUDE_DIR}/catalist/core/version.hpp"
  @ONLY
)
configure_file(doc/catalist.1.in "${CMAKE_CURRENT_BINARY_DIR}/doc/catalist.1" @ONLY)

# Domain, storage, networking and configuration. Must stay free of FTXUI so
# the CLI and the tests can be built and run without a terminal UI.
add_library(catalistCore STATIC
  src/config/config.cpp
  src/config/kdl.cpp
  src/config/keybindManager.cpp
  src/core/clock.cpp
  src/core/date.cpp
  src/core/jsonLite.cpp
  src/core/logger.cpp
  src/core/shell.cpp
  src/core/url.cpp
  src/model/activity.cpp
  src/model/entry.cpp
  src/model/entryOps.cpp
  src/model/queueOrder.cpp
  src/model/undoStack.cpp
  src/net/malAuth.cpp
  src/net/metadataSync.cpp
  src/net/providers/anilistProvider.cpp
  src/net/providers/engineSearch.cpp
  src/net/providers/kitsuProvider.cpp
  src/net/providers/malProvider.cpp
  src/net/providers/providerRegistry.cpp
  src/net/remoteSearch.cpp
  src/platform/fileSystem.cpp
  src/platform/httpClient.cpp
  src/platform/processLauncher.cpp
  src/storage/backup.cpp
  src/storage/csvCodec.cpp
  src/storage/database.cpp
  src/storage/malParser.cpp
)
target_include_directories(catalistCore PUBLIC
  include
  "${CATALIST_GENERATED_INCLUDE_DIR}"
)
target_compile_options(catalistCore PRIVATE ${CATALIST_WARNING_FLAGS})

add_library(catalistCli STATIC
  src/cli/args.cpp
  src/cli/commands/add.cpp
  src/cli/commands/completion.cpp
  src/cli/commands/config.cpp
  src/cli/commands/count.cpp
  src/cli/commands/export.cpp
  src/cli/commands/import.cpp
  src/cli/commands/ls.cpp
  src/cli/commands/random.cpp
  src/cli/commands/rm.cpp
  src/cli/commands/search.cpp
  src/cli/commands/set.cpp
  src/cli/commands/show.cpp
  src/cli/commands/stats.cpp
  src/cli/dispatcher.cpp
  src/cli/output.cpp
  src/cli/resolveEntry.cpp
)
target_link_libraries(catalistCli PUBLIC catalistCore)
target_compile_options(catalistCli PRIVATE ${CATALIST_WARNING_FLAGS})

add_library(catalistTui STATIC
  src/tui/actions.cpp
  src/tui/app.cpp
  src/tui/keys.cpp
  src/tui/jobRunner.cpp
  src/tui/tab.cpp
  src/tui/tabRegistry.cpp
  src/tui/tabs/catalogTab.cpp
  src/tui/tabs/helpTab.cpp
  src/tui/tabs/historyTab.cpp
  src/tui/tabs/libraryTab.cpp
  src/tui/tabs/queueTab.cpp
  src/tui/tabs/searchTab.cpp
  src/tui/tabs/settingsTab.cpp
  src/tui/tabs/statsTab.cpp
  src/tui/theme/statusStyle.cpp
  src/tui/theme/themeRegistry.cpp
  src/tui/widgets/commandPalette.cpp
  src/tui/widgets/marquee.cpp
  src/tui/widgets/notifier.cpp
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
target_link_libraries(catalist PRIVATE catalistCore catalistCli catalistTui)
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
  tests/activityTest.cpp
  tests/cliGoldenTest.cpp
  tests/cliSetTest.cpp
  tests/cliTest.cpp
  tests/configTest.cpp
  tests/csvCodecTest.cpp
  tests/databaseTest.cpp
  tests/dateTest.cpp
  tests/entryOpsTest.cpp
  tests/entryTest.cpp
  tests/fileSystemTest.cpp
  tests/jsonLiteTest.cpp
  tests/kdlTest.cpp
  tests/keybindManagerTest.cpp
  tests/loggerTest.cpp
  tests/malAuthTest.cpp
  tests/malParserTest.cpp
  tests/providerTest.cpp
  tests/queueOrderTest.cpp
  tests/remoteSearchTest.cpp
  tests/resolveEntryTest.cpp
  tests/resultTest.cpp
  tests/shellTest.cpp
  tests/support/sampleLibrary.cpp
  tests/support/testSupport.cpp
  tests/testMain.cpp
  tests/undoStackTest.cpp
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
  catalistCli
)
target_compile_options(catalistTests PRIVATE ${CATALIST_WARNING_FLAGS})

doctest_discover_tests(catalistTests)
