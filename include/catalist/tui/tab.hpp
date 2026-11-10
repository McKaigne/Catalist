#ifndef CATALIST_TUI_TAB_HPP_
#define CATALIST_TUI_TAB_HPP_

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <vector>

#include "catalist/tui/appContext.hpp"

namespace catalist {

struct hint {
  std::string key;
  std::string description;
};

// What the keyboard currently does, shown as a pill in the bottom-left corner:
// normal, search, edit, visual selection or a pending delete.
enum class appMode { kNor, kSch, kEdt, kVis, kDel };

class iTab {
public:
  virtual ~iTab() = default;

  [[nodiscard]] virtual std::string id() const = 0;
  [[nodiscard]] virtual std::string title() const = 0;
  [[nodiscard]] virtual std::string icon() const = 0;

  virtual ftxui::Element render(appContext& ctx) = 0;
  virtual bool onEvent(const ftxui::Event& event, appContext& ctx) = 0;
  [[nodiscard]] virtual std::vector<hint> hints() const = 0;

  virtual void onActivate(appContext&) {}
  virtual void onDeactivate(appContext&) {}
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
  [[nodiscard]] virtual std::string selectionSummary() const { return ""; }
};

}  // namespace catalist

#endif  // CATALIST_TUI_TAB_HPP_
