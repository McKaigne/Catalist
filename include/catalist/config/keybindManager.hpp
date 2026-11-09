#ifndef CATALIST_KEYBIND_MANAGER_HPP_
#define CATALIST_KEYBIND_MANAGER_HPP_

#include <string>
#include <unordered_map>

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
  kQuit
};

class keybindManager {
 public:
  keybindManager();

  void setBinding(const std::string& actionName, const std::string& key);
  std::string getKey(keyAction action) const;
  bool matches(keyAction action, const std::string& key) const;

 private:
  std::unordered_map<keyAction, std::string> primaryBindings_;
  void loadDefaults();
};

}  // namespace catalist

#endif  // CATALIST_KEYBIND_MANAGER_HPP_
