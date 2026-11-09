#include "catalist/tui/tabs/aboutTab.hpp"

#include "catalist/core/version.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"

namespace catalist {

using namespace ftxui;

std::string aboutTab::id() const {
  return "about";
}

std::string aboutTab::title() const {
  return "Catalist";
}

std::string aboutTab::icon() const {
  return icon::kAppLogo;
}

Element aboutTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  auto cardColor = [&](int card) {
    return color(activeCard_ == card ? theme.mauve : theme.surface0);
  };

  auto intro =
      vbox({text(" 󰄛 Welcome to Catalist") | bold | color(theme.mauve),
            text(""), text(" Catalist is a fast, lightweight anime"),
            text(" and manga tracker for your terminal."), text(""),
            text(" Log watched episodes and read chapters,"),
            text(" rate your favorite series, and browse"),
            text(" your library with zero clutter or ads."), text(""),
            text(std::string(" Version: v") + kVersionString) |
                color(theme.overlay0),
            filler()}) |
      border | cardColor(0) | size(WIDTH, EQUAL, 34);

  auto storage =
      vbox({text(" 󰓹 Private & Local Storage") | bold | color(theme.peach),
            text(""), text(" Your watchlist is saved locally on"),
            text(" your computer in simple CSV files."), text(""),
            text("  • Total Anime : " +
                 std::to_string(ctx.db.getEntries(mediaType::kAnime).size())),
            text("  • Total Manga : " +
                 std::to_string(ctx.db.getEntries(mediaType::kManga).size())),
            text(""), text(" Location:"),
            text(" ~/.config/catalist/") | color(theme.subtext), text(""),
            text(" No accounts or tracking required.") | color(theme.green),
            filler()}) |
      border | cardColor(1) | size(WIDTH, EQUAL, 36);

  auto online =
      vbox({text(" 󰖟 Online Discovery & Sync") | bold | color(theme.lavender),
            text(""), text(" Search online libraries instantly"),
            text(" by pressing [a] from the catalog."), text(""),
            text(" Pull episode totals, synopsis, and"),
            text(" scores directly from MyAnimeList"),
            text(" and AniList without leaving the app."), text(""),
            text(" Reconcile ongoing series in Sync.") | color(theme.subtext),
            filler()}) |
      border | cardColor(2) | size(WIDTH, EQUAL, 34);

  return vbox({text(""),
               hbox({intro, text(" "), storage, text(" "), online}) | center |
                   flex,
               text(""),
               text(" [h/l] Switch Card  │  [Tab] Next Tab ") |
                   color(theme.overlay0) | center,
               filler()}) |
         center | flex;
}

bool aboutTab::onEvent(const Event& event, appContext& ctx) {
  if (keys::isMoveLeft(event, ctx.config.keybinds)) {
    activeCard_ = std::max(0, activeCard_ - 1);
    return true;
  }
  if (keys::isMoveRight(event, ctx.config.keybinds)) {
    activeCard_ = std::min(2, activeCard_ + 1);
    return true;
  }
  return false;
}

std::vector<hint> aboutTab::hints() const {
  return {{"h/l", "card"}, {"Tab", "tabs"}, {"q", "quit"}};
}

}  // namespace catalist
