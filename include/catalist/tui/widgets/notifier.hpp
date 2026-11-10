#ifndef CATALIST_TUI_WIDGETS_NOTIFIER_HPP_
#define CATALIST_TUI_WIDGETS_NOTIFIER_HPP_

#include <deque>
#include <ftxui/dom/elements.hpp>
#include <mutex>
#include <optional>
#include <string>

#include "catalist/tui/theme/palettes.hpp"

namespace catalist {

// The one-line status message in the bottom bar plus a queue of popups that
// must be dismissed one by one. Popups may be pushed from background threads.
class notifier {
public:
  void setStatus(std::string message);
  [[nodiscard]] std::string status() const;

  void push(std::string message);
  [[nodiscard]] bool hasPopup() const;
  void dismiss();

  [[nodiscard]] std::optional<ftxui::Element> renderPopup() const;

private:
  // Guards popups_ and status_: the background sync thread pushes popups
  // while the UI thread renders.
  mutable std::mutex mutex_;
  std::deque<std::string> popups_;
  std::string status_ = "Ready.";
};

}  // namespace catalist

#endif  // CATALIST_TUI_WIDGETS_NOTIFIER_HPP_
