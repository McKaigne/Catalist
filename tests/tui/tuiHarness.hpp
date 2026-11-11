#ifndef CATALIST_TESTS_TUI_TUI_HARNESS_HPP_
#define CATALIST_TESTS_TUI_TUI_HARNESS_HPP_

#include <filesystem>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "support/testSupport.hpp"
#include "tui/app.hpp"

namespace catalist::test {

// A complete appContext for tab tests: the catalogue in a temporary
// directory, the fixed test date and a web that is offline until told
// otherwise.
struct tuiHarness : tempDirFixture {
  tuiHarness();

  bool press(iTab& tab, const ftxui::Event& event);
  bool type(iTab& tab, const std::string& keys);
  [[nodiscard]] std::string screen(iTab& tab);
  [[nodiscard]] std::filesystem::path dataDir() const;

  fakeWeb web;
  catalogue db;
  appContext ctx;
};

// Draws an element on a 200x50 screen and returns the text.
[[nodiscard]] std::string draw(const ftxui::Element& element);

// The screen line on which `needle` first appears, or -1 when it is not shown.
[[nodiscard]] int lineOf(const std::string& screenText,
                         const std::string& needle);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_TUI_TUI_HARNESS_HPP_
