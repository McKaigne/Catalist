#pragma once

#include <string>
#include <unordered_map>

namespace catalist {

enum class KeyAction {
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
  CommandPalette,
  OpenHelp,
  Quit
};

class KeybindManager {
public:
  KeybindManager();

  void setBinding(const std::string& actionName, const std::string& key);
  std::string getKey(KeyAction action) const;
  bool matches(KeyAction action, const std::string& key) const;

private:
  std::unordered_map<KeyAction, std::string> primaryBindings_;
  void loadDefaults();
};

} // namespace catalist
