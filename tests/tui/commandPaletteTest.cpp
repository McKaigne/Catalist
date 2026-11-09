#include <doctest/doctest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/commandPalette.hpp"

using catalist::commandPalette;
using ftxui::Event;

namespace {

std::string rendered(const commandPalette& palette) {
  auto canvas = ftxui::Screen::Create(ftxui::Dimension::Fixed(120),
                                      ftxui::Dimension::Fixed(30));
  ftxui::Render(canvas, palette.render(catalist::themeRegistry::active()));
  return canvas.ToString();
}

}  // namespace

TEST_CASE("tui command palette types and executes the query") {
  commandPalette palette;
  std::string executed;
  auto runner = [&](const std::string& command) { executed = command; };
  palette.open();
  REQUIRE(palette.isOpen());
  for (char key : std::string("backup"))
    palette.onEvent(Event::Character(key), runner);
  palette.onEvent(Event::Return, runner);
  CHECK(executed == "backup");
  CHECK_FALSE(palette.isOpen());
}

TEST_CASE("tui command palette tab completes the selection and lists themes") {
  commandPalette palette;
  std::string executed;
  auto runner = [&](const std::string& command) { executed = command; };
  palette.open();
  for (char key : std::string("them"))
    palette.onEvent(Event::Character(key), runner);
  palette.onEvent(Event::Tab, runner);
  CHECK(rendered(palette).find("theme nord") != std::string::npos);
  palette.onEvent(Event::ArrowDown, runner);
  palette.onEvent(Event::Backspace, runner);
  palette.onEvent(Event::Escape, runner);
  CHECK_FALSE(palette.isOpen());
  CHECK(executed.empty());
}

TEST_CASE("tui command palette runs the highlighted command on an empty query") {
  commandPalette palette;
  std::string executed;
  palette.open();
  palette.onEvent(Event::Return, [&](const std::string& command) {
    executed = command;
  });
  CHECK(executed == "theme");
}
