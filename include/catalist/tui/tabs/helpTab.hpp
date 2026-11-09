#ifndef CATALIST_TUI_TABS_HELP_TAB_HPP_
#define CATALIST_TUI_TABS_HELP_TAB_HPP_

#include <string>
#include <vector>

#include "catalist/tui/tab.hpp"

namespace catalist {

// Keyboard reference built from the live keybindings, plus the about card.
class helpTab : public iTab {
public:
  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;

  // Titles in top-bar order, so "1..9" in the reference match the bar.
  void setTabTitles(std::vector<std::string> tabTitles);

private:
  std::vector<std::string> tabTitles_;
  int scroll_ = 0;
};

// One-line summary used by the ':about' command.
[[nodiscard]] std::string aboutLine(appContext& ctx);

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_HELP_TAB_HPP_
