#ifndef CATALIST_TUI_TABS_HISTORY_TAB_HPP_
#define CATALIST_TUI_TABS_HISTORY_TAB_HPP_

#include <map>
#include <vector>

#include "catalist/model/activity.hpp"
#include "catalist/tui/tab.hpp"

namespace catalist {

// An activity heatmap of recent weeks above a day-by-day log.
class historyTab : public iTab {
public:
  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;
  void onActivate(appContext& ctx) override;

private:
  ftxui::Element renderHeatmap(const isoDate& today) const;

  std::vector<historyDay> days_;
  std::map<long, size_t> countsByDay_;
  int cursor_ = 0;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_HISTORY_TAB_HPP_
