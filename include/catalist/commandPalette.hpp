#ifndef CATALIST_COMMAND_PALETTE_HPP_
#define CATALIST_COMMAND_PALETTE_HPP_

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <string>
#include <vector>

#include "catalist/themeRegistry.hpp"

namespace catalist {

struct PaletteItem {
  std::string command;
  std::string description;
  std::string argsHint;
};

class CommandPalette {
 public:
  CommandPalette();

  void open(const std::string& initialPrefix = "");
  void close();
  bool isOpen() const;

  void handleEvent(const std::string& keyChar, bool isBack, bool isEnter,
                   bool isTab, bool isUp, bool isDown);
  void execute(const std::function<void(const std::string&)>& onExecute);

  ftxui::Element render(const ThemePalette& theme);

 private:
  bool isOpen_ = false;
  std::string query_;
  int selectedIndex_ = 0;
  std::vector<PaletteItem> catalog_;
  std::vector<PaletteItem> filteredItems_;

  void updateFilter();
  void initCatalog();
};

}  // namespace catalist

#endif  // CATALIST_COMMAND_PALETTE_HPP_
