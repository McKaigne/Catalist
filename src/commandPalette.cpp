#include "catalist/commandPalette.hpp"
#include <algorithm>
#include <sstream>

namespace catalist {

using namespace ftxui;

CommandPalette::CommandPalette() {
  initCatalog();
}

void CommandPalette::initCatalog() {
  catalog_ = {
    {"theme", "Switch color palette", "<catppuccin|dracula|nord|tokyo-night...>"},
    {"sort", "Sort table by property", "<score|title|date|progress|status|id>"},
    {"reverse", "Invert sort order", ""},
    {"add", "Search MyAnimeList and add entry", "<title>"},
    {"filter", "Filter catalog by title", "<query>"},
    {"clear", "Clear active filter", ""},
    {"note", "Set notes on active entry", "<text>"},
    {"rename", "Rename active entry", "<new_title>"},
    {"open", "Open active entry in browser", ""},
    {"copy", "Copy active entry title to clipboard", ""},
    {"sync", "Reconcile finished episode counts", ""},
    {"backup", "Generate snapshot backup CSV", ""},
    {"export", "Dump database to stdout", "<json|csv>"},
    {"undo", "Undo last database mutation", ""},
    {"redo", "Redo last undone mutation", ""},
    {"write", "Save database to disk", ""},
    {"quit", "Exit Catalist", ""},
    {"wq", "Save and exit Catalist", ""},
    {"help", "Toggle keybindings cheat-sheet", ""}
  };
}

void CommandPalette::open(const std::string& initialPrefix) {
  isOpen_ = true;
  query_ = initialPrefix;
  selectedIndex_ = 0;
  updateFilter();
}

void CommandPalette::close() {
  isOpen_ = false;
  query_.clear();
  filteredItems_.clear();
}

bool CommandPalette::isOpen() const {
  return isOpen_;
}

void CommandPalette::updateFilter() {
  filteredItems_.clear();
  std::string q = query_;
  std::transform(q.begin(), q.end(), q.begin(), ::tolower);

  std::stringstream ss(q);
  std::string verb;
  ss >> verb;

  if (verb == "theme") {
    auto themes = ThemeRegistry::getAvailableThemes();
    std::string arg;
    ss >> arg;
    for (const auto& th : themes) {
      if (arg.empty() || th.find(arg) != std::string::npos) {
        filteredItems_.push_back({"theme " + th, "Set theme to " + th, ""});
      }
    }
    selectedIndex_ = std::clamp(selectedIndex_, 0, std::max(0, static_cast<int>(filteredItems_.size()) - 1));
    return;
  }

  for (const auto& item : catalog_) {
    if (q.empty() || item.command.find(verb) != std::string::npos || item.description.find(q) != std::string::npos) {
      filteredItems_.push_back(item);
    }
  }

  selectedIndex_ = std::clamp(selectedIndex_, 0, std::max(0, static_cast<int>(filteredItems_.size()) - 1));
}

void CommandPalette::handleEvent(const std::string& keyChar, bool isBack, bool isEnter, bool isTab, bool isUp, bool isDown) {
  (void)isEnter;
  if (isBack) {
    if (!query_.empty()) {
      query_.pop_back();
      updateFilter();
    }
    return;
  }

  if (isTab) {
    if (!filteredItems_.empty() && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(filteredItems_.size())) {
      query_ = filteredItems_[selectedIndex_].command + " ";
      updateFilter();
    }
    return;
  }

  if (isUp) {
    if (selectedIndex_ > 0) selectedIndex_--;
    return;
  }

  if (isDown) {
    if (selectedIndex_ < static_cast<int>(filteredItems_.size()) - 1) selectedIndex_++;
    return;
  }

  if (!keyChar.empty()) {
    query_ += keyChar;
    updateFilter();
  }
}

void CommandPalette::execute(const std::function<void(const std::string&)>& onExecute) {
  std::string cmd = query_;
  if (cmd.empty() && !filteredItems_.empty() && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(filteredItems_.size())) {
    cmd = filteredItems_[selectedIndex_].command;
  }
  close();
  if (!cmd.empty()) {
    onExecute(cmd);
  }
}

Element CommandPalette::render(const ThemePalette& theme) {
  Elements items;

  for (size_t i = 0; i < filteredItems_.size(); ++i) {
    const auto& it = filteredItems_[i];
    bool active = (static_cast<int>(i) == selectedIndex_);

    auto row = hbox({
      text(active ? " ▌ " : "   ") | color(theme.mauve) | bold,
      text(it.command) | bold | color(active ? theme.peach : theme.text) | size(WIDTH, EQUAL, 24),
      text(" " + it.description) | color(theme.subtext) | flex,
      text(it.argsHint.empty() ? "" : (" " + it.argsHint + " ")) | color(theme.overlay0)
    });

    if (active) {
      row = row | bgcolor(theme.surface0);
    }
    items.push_back(row);
  }

  if (items.empty()) {
    items.push_back(text(" No matching commands.") | color(theme.subtext) | center);
  }

  auto inputBar = hbox({
    text(" :") | bold | color(theme.peach),
    text(query_) | color(theme.text) | bold,
    text("█") | color(theme.mauve) | blink,
    filler()
  });

  auto modalInputCard = vbox({
    inputBar
  }) | border | color(theme.peach)
     | size(HEIGHT, EQUAL, 3);

  auto modalPickerCard = vbox({
    vbox(std::move(items)) | vscroll_indicator | frame | flex
  }) | border | color(theme.mauve)
     | size(HEIGHT, EQUAL, 14);

  return vbox({
    hbox({
      text("  󰌌 Command Palette ") | bold | color(theme.mauve),
      filler(),
      text(" [Esc] Close ") | color(theme.subtext)
    }),
    text(""),
    modalInputCard,
    text(""),
    modalPickerCard,
    text(""),
    text(" [ret] Execute  │  [Tab] Complete  │  [↑/↓] Select  │  [Esc] Close ") | color(theme.subtext)
  }) | border
     | color(theme.mauve)
     | bgcolor(theme.base)
     | size(WIDTH, EQUAL, 86)
     | clear_under
     | center;
}

} // namespace catalist
