#ifndef CATALIST_TUI_APP_HPP_
#define CATALIST_TUI_APP_HPP_

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <vector>

#include "catalist/tui/appContext.hpp"
#include "catalist/tui/tab.hpp"

namespace catalist {

// The frame around the tabs: the tab bar on top, the mode, status message and
// key hints at the bottom, and the keys that work in every tab.
class appShell {
 public:
  appShell(appContext& ctx, std::vector<iTab*> tabs, std::function<void()> quit);

  [[nodiscard]] int& activeIndex() { return activeIndex_; }
  void activate(int index);

  // Tab, Shift-Tab and the number keys switch tabs and q quits, unless the
  // active tab is taking text; everything else goes to the active tab.
  bool onEvent(const ftxui::Event& event);
  [[nodiscard]] ftxui::Element render();

 private:
  [[nodiscard]] iTab& activeTab() { return *tabs_[activeIndex_]; }
  bool handleGlobalKey(const ftxui::Event& event);
  [[nodiscard]] ftxui::Element renderTopBar();
  [[nodiscard]] ftxui::Element renderBottomBar();

  appContext& ctx_;
  std::vector<iTab*> tabs_;
  std::function<void()> quit_;
  int activeIndex_ = 0;
};

// Runs the full-screen interface until the user quits.
void runTui(database& db, const iClock& clock, iProcessLauncher& launcher);

}  // namespace catalist

#endif  // CATALIST_TUI_APP_HPP_
