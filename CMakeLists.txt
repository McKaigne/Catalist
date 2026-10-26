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

add_executable(catalist
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
  src/tui.cpp
  src/provider.cpp
  src/main.cpp
)

target_link_libraries(catalist PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

install(TARGETS catalist DESTINATION bin)

enable_testing()
add_executable(catalist_tests
  tests/testMain.cpp
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

target_link_libraries(catalist_tests PRIVATE
  ftxui::screen
  ftxui::dom
)

add_test(NAME unit_tests COMMAND catalist_tests)
