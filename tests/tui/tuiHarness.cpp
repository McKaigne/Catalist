#include "tui/tuiHarness.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include "catalist/tui/theme/themeRegistry.hpp"

namespace catalist::test {

tuiHarness::tuiHarness()
    : clock(makeTestClock()),
      db(fs, clock, "/data"),
      http(std::make_shared<mockHttpClient>()),
      providers(http),
      auth(fs, *http, launcher, authServer, clock, "/data/mal_auth.json"),
      jobs(notices, [] {}),
      ctx{db,    config, undo,  notices,   launcher,
          clock, fs,     *http, providers, auth,
          jobs} {
  themeRegistry::setActive(config.theme);
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
  auto canvas = ftxui::Screen::Create(ftxui::Dimension::Fixed(200),
                                      ftxui::Dimension::Fixed(50));
  ftxui::Render(canvas, tab.render(ctx));
  return canvas.ToString();
}

}  // namespace catalist::test
