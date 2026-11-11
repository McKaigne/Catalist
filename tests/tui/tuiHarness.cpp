#include "tui/tuiHarness.hpp"

#include <ftxui/screen/screen.hpp>

namespace catalist::test {

tuiHarness::tuiHarness()
    : db(tempDir() / "data"), ctx{db, testToday, web.fetcher()} {}

std::filesystem::path tuiHarness::dataDir() const {
  return tempDir() / "data";
}

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
  return draw(tab.render(ctx));
}

std::string draw(const ftxui::Element& element) {
  auto canvas = ftxui::Screen::Create(ftxui::Dimension::Fixed(200),
                                      ftxui::Dimension::Fixed(50));
  ftxui::Render(canvas, element);
  return canvas.ToString();
}

int lineOf(const std::string& screenText, const std::string& needle) {
  auto found = screenText.find(needle);
  if (found == std::string::npos)
    return -1;
  int line = 0;
  for (size_t i = 0; i < found; ++i)
    line += screenText[i] == '\n' ? 1 : 0;
  return line;
}

}  // namespace catalist::test
