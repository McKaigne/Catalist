#ifndef CATALIST_TUI_TABS_SETTINGS_TAB_HPP_
#define CATALIST_TUI_TABS_SETTINGS_TAB_HPP_

#include "catalist/tui/tab.hpp"

namespace catalist {

class settingsTab : public iTab {
public:
  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;

private:
  enum class panel { kThemes, kPreferences };
  void applySelectedTheme(appContext& ctx);
  void cyclePreference(appContext& ctx);

  panel activePanel_ = panel::kThemes;
  int themeCursor_ = 0;
  int preferenceCursor_ = 0;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_SETTINGS_TAB_HPP_
