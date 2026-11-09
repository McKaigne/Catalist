#ifndef CATALIST_KEYBIND_MANAGER_HPP_
#define CATALIST_KEYBIND_MANAGER_HPP_

#include <string>
#include <unordered_map>

namespace catalist {

enum class keyAction {
  MoveUp,
  MoveDown,
  JumpTop,
  JumpBottom,
  FocusLeft,
  FocusRight,
  CyclePaneNext,
  CyclePanePrev,
  IncrementProgress,
  DecrementProgress,
  CycleStatus,
  TabAnime,
  TabManga,
  TabOverview,
  InspectDetails,
  EditNotes,
  RenameTitle,
  AddEntry,
  DeleteEntry,
  SortDate,
  SortTitle,
  SortStatus,
  SortRating,
  SortProgress,
  SearchFilter,
  OpenBrowser,
  YankTitle,
  Undo,
  Redo,
  commandPalette,
  OpenHelp,
  Quit
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
