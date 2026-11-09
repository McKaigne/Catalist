#include "catalist/keybindManager.hpp"

#include <algorithm>

namespace catalist {

keybindManager::keybindManager() {
  loadDefaults();
}

void keybindManager::loadDefaults() {
  primaryBindings_[keyAction::kMoveUp] = "k";
  primaryBindings_[keyAction::kMoveDown] = "j";
  primaryBindings_[keyAction::kJumpTop] = "g";
  primaryBindings_[keyAction::kJumpBottom] = "G";
  primaryBindings_[keyAction::kFocusLeft] = "h";
  primaryBindings_[keyAction::kFocusRight] = "l";
  primaryBindings_[keyAction::kCyclePaneNext] = "Tab";
  primaryBindings_[keyAction::kCyclePanePrev] = "BackTab";
  primaryBindings_[keyAction::kIncrementProgress] = "+";
  primaryBindings_[keyAction::kDecrementProgress] = "-";
  primaryBindings_[keyAction::kCycleStatus] = "Space";
  primaryBindings_[keyAction::kTabAnime] = "1";
  primaryBindings_[keyAction::kTabManga] = "2";
  primaryBindings_[keyAction::kTabOverview] = "3";
  primaryBindings_[keyAction::kInspectDetails] = "Enter";
  primaryBindings_[keyAction::kEditNotes] = "e";
  primaryBindings_[keyAction::kRenameTitle] = "t";
  primaryBindings_[keyAction::kAddEntry] = "a";
  primaryBindings_[keyAction::kDeleteEntry] = "d";
  primaryBindings_[keyAction::kSortDate] = "c";
  primaryBindings_[keyAction::kSortTitle] = "t";
  primaryBindings_[keyAction::kSortStatus] = "s";
  primaryBindings_[keyAction::kSortRating] = "r";
  primaryBindings_[keyAction::kSortProgress] = "p";
  primaryBindings_[keyAction::kSearchFilter] = "/";
  primaryBindings_[keyAction::kOpenBrowser] = "o";
  primaryBindings_[keyAction::kYankTitle] = "y";
  primaryBindings_[keyAction::kUndo] = "u";
  primaryBindings_[keyAction::kRedo] = "U";
  primaryBindings_[keyAction::kCommandPalette] = ":";
  primaryBindings_[keyAction::kOpenHelp] = "?";
  primaryBindings_[keyAction::kQuit] = "q";
}

void keybindManager::setBinding(const std::string& actionName,
                                const std::string& key) {
  std::string lower = actionName;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "moveup")
    primaryBindings_[keyAction::kMoveUp] = key;
  else if (lower == "movedown")
    primaryBindings_[keyAction::kMoveDown] = key;
  else if (lower == "jumptop")
    primaryBindings_[keyAction::kJumpTop] = key;
  else if (lower == "jumpbottom")
    primaryBindings_[keyAction::kJumpBottom] = key;
  else if (lower == "focusleft")
    primaryBindings_[keyAction::kFocusLeft] = key;
  else if (lower == "focusright")
    primaryBindings_[keyAction::kFocusRight] = key;
  else if (lower == "cyclepanenext" || lower == "cyclepane")
    primaryBindings_[keyAction::kCyclePaneNext] = key;
  else if (lower == "cyclepaneprev")
    primaryBindings_[keyAction::kCyclePanePrev] = key;
  else if (lower == "increment" || lower == "incrementprogress")
    primaryBindings_[keyAction::kIncrementProgress] = key;
  else if (lower == "decrement" || lower == "decrementprogress")
    primaryBindings_[keyAction::kDecrementProgress] = key;
  else if (lower == "cyclestatus")
    primaryBindings_[keyAction::kCycleStatus] = key;
  else if (lower == "tabanime")
    primaryBindings_[keyAction::kTabAnime] = key;
  else if (lower == "tabmanga")
    primaryBindings_[keyAction::kTabManga] = key;
  else if (lower == "taboverview")
    primaryBindings_[keyAction::kTabOverview] = key;
  else if (lower == "inspect" || lower == "inspectdetails")
    primaryBindings_[keyAction::kInspectDetails] = key;
  else if (lower == "editnotes")
    primaryBindings_[keyAction::kEditNotes] = key;
  else if (lower == "renametitle" || lower == "rename")
    primaryBindings_[keyAction::kRenameTitle] = key;
  else if (lower == "add" || lower == "addentry")
    primaryBindings_[keyAction::kAddEntry] = key;
  else if (lower == "delete" || lower == "deleteentry")
    primaryBindings_[keyAction::kDeleteEntry] = key;
  else if (lower == "sortdate")
    primaryBindings_[keyAction::kSortDate] = key;
  else if (lower == "sorttitle")
    primaryBindings_[keyAction::kSortTitle] = key;
  else if (lower == "sortstatus")
    primaryBindings_[keyAction::kSortStatus] = key;
  else if (lower == "sortrating")
    primaryBindings_[keyAction::kSortRating] = key;
  else if (lower == "sortprogress")
    primaryBindings_[keyAction::kSortProgress] = key;
  else if (lower == "search" || lower == "searchfilter")
    primaryBindings_[keyAction::kSearchFilter] = key;
  else if (lower == "openbrowser")
    primaryBindings_[keyAction::kOpenBrowser] = key;
  else if (lower == "yank" || lower == "yanktitle")
    primaryBindings_[keyAction::kYankTitle] = key;
  else if (lower == "undo")
    primaryBindings_[keyAction::kUndo] = key;
  else if (lower == "redo")
    primaryBindings_[keyAction::kRedo] = key;
  else if (lower == "commandpalette")
    primaryBindings_[keyAction::kCommandPalette] = key;
  else if (lower == "openhelp" || lower == "help")
    primaryBindings_[keyAction::kOpenHelp] = key;
  else if (lower == "quit")
    primaryBindings_[keyAction::kQuit] = key;
}

std::string keybindManager::getKey(keyAction action) const {
  auto it = primaryBindings_.find(action);
  if (it != primaryBindings_.end())
    return it->second;
  return "";
}

bool keybindManager::matches(keyAction action, const std::string& key) const {
  auto it = primaryBindings_.find(action);
  if (it != primaryBindings_.end()) {
    if (it->second == key)
      return true;
    if (it->second == "+" && (key == "+" || key == "="))
      return true;
  }
  return false;
}

}  // namespace catalist
