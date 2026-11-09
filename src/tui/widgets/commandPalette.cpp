#include "catalist/tui/widgets/commandPalette.hpp"

#include <algorithm>
#include <sstream>

namespace catalist {

using namespace ftxui;

commandPalette::commandPalette() {
  initCatalog();
}

void commandPalette::initCatalog() {
  catalog_ = {{"theme", "Switch color palette",
               "<catppuccin|dracula|nord|tokyo-night...>"},
              {"sort", "Sort table by property",
               "<score|title|date|progress|status|id>"},
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
              {"help", "Toggle keybindings cheat-sheet", ""}};
}

void commandPalette::open(const std::string& initialPrefix) {
  isOpen_ = true;
  query_ = initialPrefix;
  selectedIndex_ = 0;
  updateFilter();
}

void commandPalette::close() {
  isOpen_ = false;
  query_.clear();
  filteredItems_.clear();
}

bool commandPalette::isOpen() const {
  return isOpen_;
}

void commandPalette::updateFilter() {
  filteredItems_.clear();
  std::string q = query_;
  std::transform(q.begin(), q.end(), q.begin(), ::tolower);

  std::stringstream ss(q);
  std::string verb;
  ss >> verb;

  if (verb == "theme") {
    auto themes = themeRegistry::getAvailableThemes();
    std::string arg;
    ss >> arg;
    for (const auto& th : themes) {
      if (arg.empty() || th.find(arg) != std::string::npos) {
        filteredItems_.push_back({"theme " + th, "Set theme to " + th, ""});
      }
    }
    selectedIndex_ =
        std::clamp(selectedIndex_, 0,
                   std::max(0, static_cast<int>(filteredItems_.size()) - 1));
    return;
  }

  for (const auto& item : catalog_) {
    if (q.empty() || item.command.find(verb) != std::string::npos ||
        item.description.find(q) != std::string::npos) {
      filteredItems_.push_back(item);
    }
  }

  selectedIndex_ =
      std::clamp(selectedIndex_, 0,
                 std::max(0, static_cast<int>(filteredItems_.size()) - 1));
}

bool commandPalette::hasSelection() const {
  return selectedIndex_ >= 0 &&
         selectedIndex_ < static_cast<int>(filteredItems_.size());
}

void commandPalette::eraseLastCharacter() {
  if (query_.empty())
    return;
  query_.pop_back();
  updateFilter();
}

void commandPalette::completeSelection() {
  if (!hasSelection())
    return;
  query_ = filteredItems_[selectedIndex_].command + " ";
  updateFilter();
}

void commandPalette::moveSelection(int delta) {
  int last = static_cast<int>(filteredItems_.size()) - 1;
  selectedIndex_ = std::clamp(selectedIndex_ + delta, 0, std::max(0, last));
}

void commandPalette::appendText(const std::string& typed) {
  query_ += typed;
  updateFilter();
}

bool commandPalette::onEvent(
    const Event& event,
    const std::function<void(const std::string&)>& onExecute) {
  if (event == Event::Escape)
    close();
  else if (event == Event::Return)
    execute(onExecute);
  else if (event == Event::Tab)
    completeSelection();
  else if (event == Event::Backspace)
    eraseLastCharacter();
  else if (event == Event::ArrowUp)
    moveSelection(-1);
  else if (event == Event::ArrowDown)
    moveSelection(1);
  else if (event.is_character())
    appendText(event.character());
  return true;
}

void commandPalette::execute(
    const std::function<void(const std::string&)>& onExecute) {
  std::string command = query_;
  if (command.empty() && hasSelection())
    command = filteredItems_[selectedIndex_].command;
  close();
  if (!command.empty())
    onExecute(command);
}

Element commandPalette::render(const themePalette& theme) const {
  Elements items;

  for (size_t i = 0; i < filteredItems_.size(); ++i) {
    const auto& it = filteredItems_[i];
    bool active = (static_cast<int>(i) == selectedIndex_);

    auto row = hbox(
        {text(active ? " ▌ " : "   ") | color(theme.mauve) | bold,
         text(it.command) | bold | color(active ? theme.peach : theme.text) |
             size(WIDTH, EQUAL, 24),
         text(" " + it.description) | color(theme.subtext) | flex,
         text(it.argsHint.empty() ? "" : (" " + it.argsHint + " ")) |
             color(theme.overlay0)});

    if (active) {
      row = row | bgcolor(theme.surface0);
    }
    items.push_back(row);
  }

  if (items.empty()) {
    items.push_back(text(" No matching commands.") | color(theme.subtext) |
                    center);
  }

  auto inputBar = hbox({text(" :") | bold | color(theme.peach),
                        text(query_) | color(theme.text) | bold,
                        text("█") | color(theme.mauve) | blink, filler()});

  auto modalInputCard =
      vbox({inputBar}) | border | color(theme.peach) | size(HEIGHT, EQUAL, 3);

  auto modalPickerCard =
      vbox({vbox(std::move(items)) | vscroll_indicator | frame | flex}) |
      border | color(theme.mauve) | size(HEIGHT, EQUAL, 14);

  return vbox(
             {hbox({text("  󰌌 Command Palette ") | bold | color(theme.mauve),
                    filler(), text(" [Esc] Close ") | color(theme.subtext)}),
              text(""), modalInputCard, text(""), modalPickerCard, text(""),
              text(
                  " [ret] Execute  │  [Tab] Complete  │  [↑/↓] Select  │  [Esc] Close ") |
                  color(theme.subtext)}) |
         border | color(theme.mauve) | bgcolor(theme.base) |
         size(WIDTH, EQUAL, 86) | clear_under | center;
}

}  // namespace catalist
