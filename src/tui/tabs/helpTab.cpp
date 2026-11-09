#include "catalist/tui/tabs/helpTab.hpp"

#include <algorithm>

#include "catalist/core/version.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"

namespace catalist {

using namespace ftxui;

namespace {

std::string describe(keyAction action) {
  switch (action) {
    case keyAction::kMoveUp: return "Move up";
    case keyAction::kMoveDown: return "Move down";
    case keyAction::kJumpTop: return "Jump to top";
    case keyAction::kJumpBottom: return "Jump to bottom";
    case keyAction::kFocusLeft: return "Pane / column left";
    case keyAction::kFocusRight: return "Pane / column right";
    case keyAction::kIncrementProgress: return "Progress +1 (also =)";
    case keyAction::kDecrementProgress: return "Progress -1";
    case keyAction::kEditNotes: return "Edit notes (details)";
    case keyAction::kAddEntry: return "Add entry (opens Search)";
    case keyAction::kDeleteEntry: return "Delete entry";
    case keyAction::kSortDate: return "Sort by date";
    case keyAction::kSortTitle: return "Sort by title";
    case keyAction::kSortStatus: return "Sort by status";
    case keyAction::kSortRating: return "Sort by rating";
    case keyAction::kSortProgress: return "Sort by progress";
    case keyAction::kSearchFilter: return "Filter / type a search";
    case keyAction::kOpenBrowser: return "Open in browser";
    case keyAction::kYankTitle: return "Copy title";
    case keyAction::kUndo: return "Undo";
    case keyAction::kRedo: return "Redo (also Ctrl-R)";
    case keyAction::kCommandPalette: return "Command palette";
    case keyAction::kOpenHelp: return "Open this help";
    case keyAction::kQuit: return "Save and quit";
    case keyAction::kMoveItemUp: return "Queue: move item up";
    case keyAction::kMoveItemDown: return "Queue: move item down";
    case keyAction::kToggleMedia: return "Toggle anime / manga";
    case keyAction::kNextProvider: return "Search: next provider";
    default: return "";
  }
}

Element keyRow(const std::string& key, const std::string& description,
               const themePalette& theme) {
  return hbox({text("  " + key) | bold | color(theme.peach) | size(WIDTH, EQUAL, 14),
               text(description) | color(theme.text)});
}

}  // namespace

std::string aboutLine(appContext& ctx) {
  return std::string("Catalist v") + kVersionString + " · " +
         std::to_string(ctx.db.getEntries(mediaType::kAnime).size()) +
         " anime · " +
         std::to_string(ctx.db.getEntries(mediaType::kManga).size()) +
         " manga · data in " + ctx.db.getConfigPath().string();
}

void helpTab::setTabTitles(std::vector<std::string> tabTitles) {
  tabTitles_ = std::move(tabTitles);
}

std::string helpTab::id() const {
  return "help";
}

std::string helpTab::title() const {
  return "Help";
}

std::string helpTab::icon() const {
  return "󰋖";
}

bool helpTab::onEvent(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  if (keys::isMoveDown(event, keybinds))
    scroll_++;
  else if (keys::isMoveUp(event, keybinds))
    scroll_ = std::max(scroll_ - 1, 0);
  else if (keys::isJumpTop(event, keybinds))
    scroll_ = 0;
  else if (keys::isJumpBottom(event, keybinds))
    scroll_ = 1000;
  else
    return false;
  return true;
}

Element helpTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  Elements global = {text(" Global") | bold | color(theme.lavender)};
  for (size_t i = 0; i < tabTitles_.size(); ++i)
    global.push_back(keyRow(std::to_string(i + 1), "Go to " + tabTitles_[i], theme));
  global.push_back(keyRow("Tab / S-Tab", "Next / previous tab", theme));
  global.push_back(keyRow(";  Esc", "Dismiss a notification", theme));

  Elements bound = {text(" Keybindings (live from catalist.kdl)") | bold |
                    color(theme.lavender)};
  for (const auto& binding : ctx.config.keybinds.bindings()) {
    std::string description = describe(binding.action);
    if (!description.empty())
      bound.push_back(keyRow(binding.key, description, theme));
  }

  auto about =
      vbox({text(" About") | bold | color(theme.lavender),
            text("  Catalist v" + std::string(kVersionString)) | color(theme.mauve),
            text("  A keyboard-driven anime and manga tracker.") |
                color(theme.text),
            text("  Data: " + ctx.db.getConfigPath().string()) |
                color(theme.subtext),
            text("  Commands: :theme :sync :backup :import :about :w :q") |
                color(theme.subtext)}) |
      border | color(theme.surface0);

  Elements content = {about, text("")};
  for (auto& line : global)
    content.push_back(std::move(line));
  content.push_back(text(""));
  for (auto& line : bound)
    content.push_back(std::move(line));
  int lastLine = static_cast<int>(content.size()) - 1;
  scroll_ = std::min(scroll_, std::max(0, lastLine));
  content[static_cast<size_t>(scroll_)] = content[static_cast<size_t>(scroll_)] | focus;
  return vbox(std::move(content)) | vscroll_indicator | frame | flex |
         size(WIDTH, EQUAL, 80) | center;
}

std::vector<hint> helpTab::hints() const {
  return {{"j/k", "scroll"}, {":", "cmd"}, {"q", "quit"}};
}

}  // namespace catalist
