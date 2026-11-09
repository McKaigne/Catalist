#ifndef CATALIST_KEYBIND_MANAGER_HPP_
#define CATALIST_KEYBIND_MANAGER_HPP_

#include <string>
#include <unordered_map>
#include <vector>

namespace catalist {

enum class keyAction {
  kMoveUp,
  kMoveDown,
  kJumpTop,
  kJumpBottom,
  kFocusLeft,
  kFocusRight,
  kCyclePaneNext,
  kCyclePanePrev,
  kIncrementProgress,
  kDecrementProgress,
  kCycleStatus,
  kTabAnime,
  kTabManga,
  kTabOverview,
  kInspectDetails,
  kEditNotes,
  kRenameTitle,
  kAddEntry,
  kDeleteEntry,
  kSortDate,
  kSortTitle,
  kSortStatus,
  kSortRating,
  kSortProgress,
  kSearchFilter,
  kOpenBrowser,
  kYankTitle,
  kUndo,
  kRedo,
  kCommandPalette,
  kOpenHelp,
  kQuit,
  kMoveItemUp,
  kMoveItemDown,
  kToggleMedia,
  kNextProvider
};

struct keyBinding {
  keyAction action;
  std::string name;
  std::string key;
};

class keybindManager {
 public:
  keybindManager();

  void setBinding(const std::string& actionName, const std::string& key);
  [[nodiscard]] std::string getKey(keyAction action) const;
  [[nodiscard]] bool matches(keyAction action, const std::string& key) const;

  // Every action with its config name and current key, in table order.
  [[nodiscard]] std::vector<keyBinding> bindings() const;

 private:
  std::unordered_map<keyAction, std::string> primaryBindings_;
};

}  // namespace catalist

#endif  // CATALIST_KEYBIND_MANAGER_HPP_
