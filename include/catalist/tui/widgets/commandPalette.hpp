#ifndef CATALIST_TUI_WIDGETS_COMMAND_PALETTE_HPP_
#define CATALIST_TUI_WIDGETS_COMMAND_PALETTE_HPP_

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <string>
#include <vector>

#include "catalist/tui/theme/themeRegistry.hpp"

namespace catalist {

struct paletteItem {
  std::string command;
  std::string description;
  std::string argsHint;
};

class commandPalette {
 public:
  commandPalette();

  void open(const std::string& initialPrefix = "");
  void close();
  [[nodiscard]] bool isOpen() const;

  // Consumes every event while open; Return runs the typed (or selected)
  // command through onExecute, Escape closes.
  bool onEvent(const ftxui::Event& event,
               const std::function<void(const std::string&)>& onExecute);

  [[nodiscard]] ftxui::Element render(const themePalette& theme) const;

 private:
  bool isOpen_ = false;
  std::string query_;
  int selectedIndex_ = 0;
  std::vector<paletteItem> catalog_;
  std::vector<paletteItem> filteredItems_;

  void updateFilter();
  void initCatalog();
  void execute(const std::function<void(const std::string&)>& onExecute);
  void completeSelection();
  void moveSelection(int delta);
  void eraseLastCharacter();
  void appendText(const std::string& typed);
  [[nodiscard]] bool hasSelection() const;
};

}  // namespace catalist

#endif  // CATALIST_TUI_WIDGETS_COMMAND_PALETTE_HPP_
