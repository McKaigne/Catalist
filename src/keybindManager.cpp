#include "catalist/keybindManager.hpp"

#include <algorithm>

namespace catalist {

keybindManager::keybindManager() {
  loadDefaults();
}

void keybindManager::loadDefaults() {
  primaryBindings_[keyAction::MoveUp] = "k";
  primaryBindings_[keyAction::MoveDown] = "j";
  primaryBindings_[keyAction::JumpTop] = "g";
  primaryBindings_[keyAction::JumpBottom] = "G";
  primaryBindings_[keyAction::FocusLeft] = "h";
  primaryBindings_[keyAction::FocusRight] = "l";
  primaryBindings_[keyAction::CyclePaneNext] = "Tab";
  primaryBindings_[keyAction::CyclePanePrev] = "BackTab";
  primaryBindings_[keyAction::IncrementProgress] = "+";
  primaryBindings_[keyAction::DecrementProgress] = "-";
  primaryBindings_[keyAction::CycleStatus] = "Space";
  primaryBindings_[keyAction::TabAnime] = "1";
  primaryBindings_[keyAction::TabManga] = "2";
  primaryBindings_[keyAction::TabOverview] = "3";
  primaryBindings_[keyAction::InspectDetails] = "Enter";
  primaryBindings_[keyAction::EditNotes] = "e";
  primaryBindings_[keyAction::RenameTitle] = "t";
  primaryBindings_[keyAction::AddEntry] = "a";
  primaryBindings_[keyAction::DeleteEntry] = "d";
  primaryBindings_[keyAction::SortDate] = "c";
  primaryBindings_[keyAction::SortTitle] = "t";
  primaryBindings_[keyAction::SortStatus] = "s";
  primaryBindings_[keyAction::SortRating] = "r";
  primaryBindings_[keyAction::SortProgress] = "p";
  primaryBindings_[keyAction::SearchFilter] = "/";
  primaryBindings_[keyAction::OpenBrowser] = "o";
  primaryBindings_[keyAction::YankTitle] = "y";
  primaryBindings_[keyAction::Undo] = "u";
  primaryBindings_[keyAction::Redo] = "U";
  primaryBindings_[keyAction::commandPalette] = ":";
  primaryBindings_[keyAction::OpenHelp] = "?";
  primaryBindings_[keyAction::Quit] = "q";
}

void keybindManager::setBinding(const std::string& actionName,
                                const std::string& key) {
  std::string lower = actionName;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "moveup")
    primaryBindings_[keyAction::MoveUp] = key;
  else if (lower == "movedown")
    primaryBindings_[keyAction::MoveDown] = key;
  else if (lower == "jumptop")
    primaryBindings_[keyAction::JumpTop] = key;
  else if (lower == "jumpbottom")
    primaryBindings_[keyAction::JumpBottom] = key;
  else if (lower == "focusleft")
    primaryBindings_[keyAction::FocusLeft] = key;
  else if (lower == "focusright")
    primaryBindings_[keyAction::FocusRight] = key;
  else if (lower == "cyclepanenext" || lower == "cyclepane")
    primaryBindings_[keyAction::CyclePaneNext] = key;
  else if (lower == "cyclepaneprev")
    primaryBindings_[keyAction::CyclePanePrev] = key;
  else if (lower == "increment" || lower == "incrementprogress")
    primaryBindings_[keyAction::IncrementProgress] = key;
  else if (lower == "decrement" || lower == "decrementprogress")
    primaryBindings_[keyAction::DecrementProgress] = key;
  else if (lower == "cyclestatus")
    primaryBindings_[keyAction::CycleStatus] = key;
  else if (lower == "tabanime")
    primaryBindings_[keyAction::TabAnime] = key;
  else if (lower == "tabmanga")
    primaryBindings_[keyAction::TabManga] = key;
  else if (lower == "taboverview")
    primaryBindings_[keyAction::TabOverview] = key;
  else if (lower == "inspect" || lower == "inspectdetails")
    primaryBindings_[keyAction::InspectDetails] = key;
  else if (lower == "editnotes")
    primaryBindings_[keyAction::EditNotes] = key;
  else if (lower == "renametitle" || lower == "rename")
    primaryBindings_[keyAction::RenameTitle] = key;
  else if (lower == "add" || lower == "addentry")
    primaryBindings_[keyAction::AddEntry] = key;
  else if (lower == "delete" || lower == "deleteentry")
    primaryBindings_[keyAction::DeleteEntry] = key;
  else if (lower == "sortdate")
    primaryBindings_[keyAction::SortDate] = key;
  else if (lower == "sorttitle")
    primaryBindings_[keyAction::SortTitle] = key;
  else if (lower == "sortstatus")
    primaryBindings_[keyAction::SortStatus] = key;
  else if (lower == "sortrating")
    primaryBindings_[keyAction::SortRating] = key;
  else if (lower == "sortprogress")
    primaryBindings_[keyAction::SortProgress] = key;
  else if (lower == "search" || lower == "searchfilter")
    primaryBindings_[keyAction::SearchFilter] = key;
  else if (lower == "openbrowser")
    primaryBindings_[keyAction::OpenBrowser] = key;
  else if (lower == "yank" || lower == "yanktitle")
    primaryBindings_[keyAction::YankTitle] = key;
  else if (lower == "undo")
    primaryBindings_[keyAction::Undo] = key;
  else if (lower == "redo")
    primaryBindings_[keyAction::Redo] = key;
  else if (lower == "commandpalette")
    primaryBindings_[keyAction::commandPalette] = key;
  else if (lower == "openhelp" || lower == "help")
    primaryBindings_[keyAction::OpenHelp] = key;
  else if (lower == "quit")
    primaryBindings_[keyAction::Quit] = key;
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
