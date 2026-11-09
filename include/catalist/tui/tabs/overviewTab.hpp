#ifndef CATALIST_TUI_TABS_OVERVIEW_TAB_HPP_
#define CATALIST_TUI_TABS_OVERVIEW_TAB_HPP_

#include "catalist/tui/tab.hpp"

namespace catalist {

class overviewTab : public iTab {
public:
  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_OVERVIEW_TAB_HPP_
