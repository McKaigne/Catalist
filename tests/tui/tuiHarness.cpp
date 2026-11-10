#include "tui/tuiHarness.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

namespace catalist::test {

tuiHarness::tuiHarness()
    : clock(makeTestClock()),
      db(fs, clock, "/data"),
      ctx{db, undo, notices, launcher, clock} {}

bool tuiHarness::press(iTab& tab, const ftxui::Event& event) {
  return tab.onEvent(event, ctx);
}

bool tuiHarness::type(iTab& tab, const std::string& keys) {
  bool handledAll = true;
  for (char key : keys)
    handledAll = press(tab, ftxui::Event::Character(key)) && handledAll;
  return handledAll;
}

std::string tuiHarness::screen(iTab& tab) {
  auto canvas = ftxui::Screen::Create(ftxui::Dimension::Fixed(200),
                                      ftxui::Dimension::Fixed(50));
  ftxui::Render(canvas, tab.render(ctx));
  return canvas.ToString();
}

}  // namespace catalist::test
