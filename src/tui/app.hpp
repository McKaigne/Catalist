#ifndef CATALIST_TUI_APP_HPP_
#define CATALIST_TUI_APP_HPP_

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "catalogue.hpp"
#include "jikan.hpp"

namespace catalist {

// Everything a tab may touch. Built once by runTui; tabs never reach for
// globals. The date and the network are functions so the tests can pin the
// one and fake the other.
struct appContext {
  catalogue& db;
  std::function<isoDate()> today;
  jikan::fetcher fetch;
  // The message in the bottom bar.
  std::string status = "Ready.";

  // Writes the catalogue after a change, then shows `done`, or the failure.
  void save(const std::string& done);
};

}  // namespace catalist

namespace catalist {

struct hint {
  std::string key;
  std::string description;
};

// What the keyboard currently does, shown as a pill in the bottom-left corner:
// normal, search, editing notes, adding a title or a pending delete.
enum class appMode { kNor, kSch, kEdt, kAdd, kDel };

class iTab {
public:
  virtual ~iTab() = default;

  [[nodiscard]] virtual std::string title() const = 0;
  [[nodiscard]] virtual std::string icon() const = 0;

  virtual ftxui::Element render(appContext& ctx) = 0;
  virtual bool onEvent(const ftxui::Event& event, appContext& ctx) = 0;
  [[nodiscard]] virtual std::vector<hint> hints() const = 0;

  virtual void onActivate(appContext&) {}
  // Called by the shell's redraw timer, four times a second, while the tab
  // is shown; where slow work (the title search) runs, so the status line
  // has been drawn before it starts.
  virtual void onTick(appContext&) {}
  // FTXUI children (menus, inputs) that need focus and default handling.
  // FTXUI only routes keys to focusable components, so tabs without children
  // get a focusable placeholder.
  virtual ftxui::Component focusRoot() {
    return ftxui::Renderer([](bool) { return ftxui::text(""); });
  }
  // True while the tab owns the keyboard (text entry, confirmations), so the
  // shell must not treat keys like '1' or 'q' as global shortcuts.
  [[nodiscard]] virtual bool capturesInput() const { return false; }
  [[nodiscard]] virtual appMode mode() const { return appMode::kNor; }
  [[nodiscard]] virtual std::optional<std::string> countBadge(
      appContext&) const {
    return std::nullopt;
  }
};

}  // namespace catalist

namespace catalist {

// The frame around the tabs: the tab bar on top, the mode, status message and
// key hints at the bottom, and the keys that work in every tab.
class appShell {
public:
  appShell(appContext& ctx, std::vector<iTab*> tabs,
           std::function<void()> quit);

  [[nodiscard]] int& activeIndex() { return activeIndex_; }
  void activate(int index);

  // Tab, Shift-Tab and the number keys switch tabs and q quits, unless the
  // active tab is taking text; everything else goes to the active tab. The
  // redraw timer's Event::Custom becomes the tab's onTick, and the mouse is
  // ignored: Catalist is driven by the keyboard.
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
void runTui(catalogue& db);

}  // namespace catalist

#endif  // CATALIST_TUI_APP_HPP_
