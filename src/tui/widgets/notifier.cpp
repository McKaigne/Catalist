#include "catalist/tui/widgets/notifier.hpp"

#include <algorithm>

namespace catalist {

using namespace ftxui;

void notifier::setStatus(std::string message) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = std::move(message);
}

std::string notifier::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_;
}

void notifier::push(std::string message) {
  std::lock_guard<std::mutex> lock(mutex_);
  popups_.push_back(std::move(message));
}

bool notifier::hasPopup() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !popups_.empty();
}

void notifier::dismiss() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!popups_.empty())
    popups_.pop_front();
}

std::optional<Element> notifier::renderPopup(const themePalette& theme) const {
  std::string message;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (popups_.empty())
      return std::nullopt;
    message = popups_.front();
  }
  int width = std::max(40, static_cast<int>(message.size()) + 8);
  return vbox({
             hbox({text("  󰎟 Notification ") | bold | color(theme.mauve),
                   filler(), text(" [;] Dismiss ") | color(theme.subtext)}),
             text(""),
             text("  " + message) | color(theme.text) | center,
             text(""),
         }) |
         border | color(theme.mauve) | bgcolor(theme.crust) |
         size(WIDTH, EQUAL, width) | clear_under | center;
}

}  // namespace catalist
