#include "tui/app.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/terminal.hpp>
#include <thread>

#include "tui/catalogTab.hpp"
#include "tui/statsTab.hpp"
#include "tui/style.hpp"
#include "version.hpp"

namespace catalist {

using namespace ftxui;

namespace {

// Redraw cadence for the scrolling titles in the catalogue; also how soon a
// title search starts after Return.
constexpr auto kTickInterval = std::chrono::milliseconds(250);

bool isBackTab(const Event& event) {
  return event == Event::TabReverse || event == Event::Special("\033[Z");
}

Element tabLabel(int number, bool active, const std::string& label,
                 const std::string& glyph, const std::string& count) {
  std::string shown = " " + std::to_string(number) + " " + glyph + " " + label;
  if (!count.empty())
    shown += " (" + count + ")";
  shown += " ";
  return text(shown) |
         (active ? bold | color(theme::kMauve) : color(theme::kSubtext));
}

std::string truncated(std::string message, int maxLength) {
  if (static_cast<int>(message.size()) <= maxLength)
    return message;
  return message.substr(0, std::max(0, maxLength - 3)) + "...";
}

Element modePill(appMode mode) {
  switch (mode) {
    case appMode::kNor:
      return pill(" NOR ", theme::kCrust, theme::kMauve);
    case appMode::kSch:
      return pill(" SCH ", theme::kCrust, theme::kPeach);
    case appMode::kEdt:
      return pill(" EDT ", theme::kCrust, theme::kGreen);
    case appMode::kAdd:
      return pill(" ADD ", theme::kCrust, theme::kBlue);
    case appMode::kDel:
      return pill(" DEL ", theme::kCrust, theme::kRed);
  }
  return text("");
}

}  // namespace

void appContext::save(const std::string& done) {
  status = db.saveAll() ? done
                        : "Could not save the catalogue to " +
                              db.getConfigPath().string() + ".";
}

appShell::appShell(appContext& ctx, std::vector<iTab*> tabs,
                   std::function<void()> quit)
    : ctx_(ctx), tabs_(std::move(tabs)), quit_(std::move(quit)) {
  activeTab().onActivate(ctx_);
}

void appShell::activate(int index) {
  if (index < 0 || index >= static_cast<int>(tabs_.size()))
    return;
  activeIndex_ = index;
  activeTab().onActivate(ctx_);
}

bool appShell::onEvent(const Event& event) {
  if (event == Event::Custom) {
    activeTab().onTick(ctx_);
    return true;
  }
  if (event.is_mouse())
    return true;
  if (!activeTab().capturesInput() && handleGlobalKey(event))
    return true;
  return activeTab().onEvent(event, ctx_);
}

bool appShell::handleGlobalKey(const Event& event) {
  int count = static_cast<int>(tabs_.size());
  if (event == Event::Tab) {
    activate((activeIndex_ + 1) % count);
    return true;
  }
  if (isBackTab(event)) {
    activate((activeIndex_ + count - 1) % count);
    return true;
  }
  for (int position = 0; position < count; ++position) {
    if (!keys::isCharacter(event, static_cast<char>('1' + position)))
      continue;
    activate(position);
    return true;
  }
  // Every change is saved as it is made, so quitting has nothing to write.
  if (keys::isCharacter(event, 'q')) {
    quit_();
    return true;
  }
  return false;
}

Element appShell::render() {
  return vbox(
      {renderTopBar(), activeTab().render(ctx_) | flex, renderBottomBar()});
}

Element appShell::renderTopBar() {
  Elements items;
  for (size_t i = 0; i < tabs_.size(); ++i) {
    iTab& tab = *tabs_[i];
    items.push_back(tabLabel(static_cast<int>(i) + 1, &tab == &activeTab(),
                             tab.title(), tab.icon(),
                             tab.countBadge(ctx_).value_or("")));
    items.push_back(text("│") | color(theme::kSurface0));
  }
  items.push_back(filler());
  items.push_back(text(std::string("v") + kVersionString + " ") |
                  color(theme::kOverlay0));
  return hbox(std::move(items)) | border | color(theme::kSurface0) |
         size(HEIGHT, EQUAL, 3);
}

Element appShell::renderBottomBar() {
  Elements hintItems;
  int hintsWidth = 0;
  for (const auto& item : activeTab().hints()) {
    hintsWidth +=
        static_cast<int>(item.key.size() + item.description.size()) + 3;
    if (!hintItems.empty())
      hintItems.push_back(text("  "));
    hintItems.push_back(
        hbox({text(item.key) | bold | color(theme::kText),
              text(" " + item.description) | color(theme::kSubtext)}));
  }

  // The pill, the separators and the borders take about 14 columns.
  int maxStatusLength = std::max(16, Terminal::Size().dimx - hintsWidth - 14);
  return hbox({text(" "), modePill(activeTab().mode()),
               text(" │ ") | color(theme::kSurface1),
               text(truncated(ctx_.status, maxStatusLength)) |
                   color(theme::kYellow),
               filler(), hbox(std::move(hintItems)), text(" ")}) |
         border | color(theme::kSurface0) | size(HEIGHT, EQUAL, 3);
}

void runTui(catalogue& db, const iClock& clock, iHttpClient& http) {
  appContext ctx{db, clock, http};

  catalogTab anime(mediaType::kAnime);
  catalogTab manga(mediaType::kManga);
  statsTab stats;
  std::vector<iTab*> tabs = {&anime, &manga, &stats};

  auto screen = ScreenInteractive::Fullscreen();
  screen.TrackMouse(false);
  appShell app(ctx, tabs, screen.ExitLoopClosure());

  // FTXUI only draws a text cursor for components it knows about, so each
  // tab's inputs sit in the tree; the shell sees every event before them.
  Components focusRoots;
  for (iTab* tab : tabs)
    focusRoots.push_back(tab->focusRoot());
  auto root =
      CatchEvent(Renderer(Container::Tab(focusRoots, &app.activeIndex()),
                          [&app] { return app.render(); }),
                 [&app](Event event) { return app.onEvent(event); });

  std::atomic<bool> running{true};
  std::thread ticker([&]() {
    while (running.load()) {
      std::this_thread::sleep_for(kTickInterval);
      screen.PostEvent(Event::Custom);
    }
  });
  screen.Loop(root);
  running = false;
  ticker.join();
}

}  // namespace catalist
