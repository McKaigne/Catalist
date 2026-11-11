#ifndef CATALIST_TESTS_TUI_TUI_HARNESS_HPP_
#define CATALIST_TESTS_TUI_TUI_HARNESS_HPP_

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "tui/app.hpp"
#include "tui/app.hpp"
#include "support/testSupport.hpp"

namespace catalist::test {

// A complete appContext over fakes: in-memory files under /data, the fixed
// test clock and an HTTP client that is offline until told otherwise.
struct tuiHarness {
  tuiHarness();

  bool press(iTab& tab, const ftxui::Event& event);
  bool type(iTab& tab, const std::string& keys);
  [[nodiscard]] std::string screen(iTab& tab);

  memoryFileSystem fs;
  fixedClock clock;
  mockHttpClient http;
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
