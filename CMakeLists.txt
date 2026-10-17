cmake_minimum_required(VERSION 3.20)
project(catalist VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

include_directories(include)

add_executable(catalist
  src/entry.cpp
  src/config.cpp
  src/database.cpp
  src/mal_parser.cpp
  src/sync.cpp
  src/tui.cpp
  src/main.cpp
)

target_link_libraries(catalist PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

install(TARGETS catalist DESTINATION bin)
