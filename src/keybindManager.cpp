#include "catalist/keybindManager.hpp"
#include <algorithm>

namespace catalist {

KeybindManager::KeybindManager() {
  loadDefaults();
}

void KeybindManager::loadDefaults() {
  primaryBindings_[KeyAction::MoveUp]            = "k";
  primaryBindings_[KeyAction::MoveDown]          = "j";
  primaryBindings_[KeyAction::JumpTop]           = "g";
  primaryBindings_[KeyAction::JumpBottom]        = "G";
  primaryBindings_[KeyAction::FocusLeft]         = "h";
  primaryBindings_[KeyAction::FocusRight]        = "l";
  primaryBindings_[KeyAction::CyclePaneNext]     = "Tab";
  primaryBindings_[KeyAction::CyclePanePrev]     = "BackTab";
  primaryBindings_[KeyAction::IncrementProgress] = "+";
  primaryBindings_[KeyAction::DecrementProgress] = "-";
  primaryBindings_[KeyAction::CycleStatus]       = "Space";
  primaryBindings_[KeyAction::TabAnime]          = "1";
  primaryBindings_[KeyAction::TabManga]          = "2";
  primaryBindings_[KeyAction::TabOverview]       = "3";
  primaryBindings_[KeyAction::InspectDetails]    = "Enter";
  primaryBindings_[KeyAction::EditNotes]         = "e";
  primaryBindings_[KeyAction::RenameTitle]       = "t";
  primaryBindings_[KeyAction::AddEntry]          = "a";
  primaryBindings_[KeyAction::DeleteEntry]       = "d";
  primaryBindings_[KeyAction::SortDate]          = "c";
  primaryBindings_[KeyAction::SortTitle]         = "t";
  primaryBindings_[KeyAction::SortStatus]        = "s";
  primaryBindings_[KeyAction::SortRating]        = "r";
  primaryBindings_[KeyAction::SortProgress]      = "p";
  primaryBindings_[KeyAction::SearchFilter]      = "/";
  primaryBindings_[KeyAction::OpenBrowser]       = "o";
  primaryBindings_[KeyAction::YankTitle]         = "y";
  primaryBindings_[KeyAction::Undo]              = "u";
  primaryBindings_[KeyAction::Redo]              = "U";
  primaryBindings_[KeyAction::CommandPalette]    = ":";
  primaryBindings_[KeyAction::OpenHelp]          = "?";
  primaryBindings_[KeyAction::Quit]              = "q";
}

void KeybindManager::setBinding(const std::string& actionName, const std::string& key) {
  std::string lower = actionName;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "moveup") primaryBindings_[KeyAction::MoveUp] = key;
  else if (lower == "movedown") primaryBindings_[KeyAction::MoveDown] = key;
  else if (lower == "jumptop") primaryBindings_[KeyAction::JumpTop] = key;
  else if (lower == "jumpbottom") primaryBindings_[KeyAction::JumpBottom] = key;
  else if (lower == "focusleft") primaryBindings_[KeyAction::FocusLeft] = key;
  else if (lower == "focusright") primaryBindings_[KeyAction::FocusRight] = key;
  else if (lower == "cyclepanenext" || lower == "cyclepane") primaryBindings_[KeyAction::CyclePaneNext] = key;
  else if (lower == "cyclepaneprev") primaryBindings_[KeyAction::CyclePanePrev] = key;
  else if (lower == "increment" || lower == "incrementprogress") primaryBindings_[KeyAction::IncrementProgress] = key;
  else if (lower == "decrement" || lower == "decrementprogress") primaryBindings_[KeyAction::DecrementProgress] = key;
  else if (lower == "cyclestatus") primaryBindings_[KeyAction::CycleStatus] = key;
  else if (lower == "tabanime") primaryBindings_[KeyAction::TabAnime] = key;
  else if (lower == "tabmanga") primaryBindings_[KeyAction::TabManga] = key;
  else if (lower == "taboverview") primaryBindings_[KeyAction::TabOverview] = key;
  else if (lower == "inspect" || lower == "inspectdetails") primaryBindings_[KeyAction::InspectDetails] = key;
  else if (lower == "editnotes") primaryBindings_[KeyAction::EditNotes] = key;
  else if (lower == "renametitle" || lower == "rename") primaryBindings_[KeyAction::RenameTitle] = key;
  else if (lower == "add" || lower == "addentry") primaryBindings_[KeyAction::AddEntry] = key;
  else if (lower == "delete" || lower == "deleteentry") primaryBindings_[KeyAction::DeleteEntry] = key;
  else if (lower == "sortdate") primaryBindings_[KeyAction::SortDate] = key;
  else if (lower == "sorttitle") primaryBindings_[KeyAction::SortTitle] = key;
  else if (lower == "sortstatus") primaryBindings_[KeyAction::SortStatus] = key;
  else if (lower == "sortrating") primaryBindings_[KeyAction::SortRating] = key;
  else if (lower == "sortprogress") primaryBindings_[KeyAction::SortProgress] = key;
  else if (lower == "search" || lower == "searchfilter") primaryBindings_[KeyAction::SearchFilter] = key;
  else if (lower == "openbrowser") primaryBindings_[KeyAction::OpenBrowser] = key;
  else if (lower == "yank" || lower == "yanktitle") primaryBindings_[KeyAction::YankTitle] = key;
  else if (lower == "undo") primaryBindings_[KeyAction::Undo] = key;
  else if (lower == "redo") primaryBindings_[KeyAction::Redo] = key;
  else if (lower == "commandpalette") primaryBindings_[KeyAction::CommandPalette] = key;
  else if (lower == "openhelp" || lower == "help") primaryBindings_[KeyAction::OpenHelp] = key;
  else if (lower == "quit") primaryBindings_[KeyAction::Quit] = key;
}

std::string KeybindManager::getKey(KeyAction action) const {
  auto it = primaryBindings_.find(action);
  if (it != primaryBindings_.end()) return it->second;
  return "";
}

bool KeybindManager::matches(KeyAction action, const std::string& key) const {
  auto it = primaryBindings_.find(action);
  if (it != primaryBindings_.end()) {
    if (it->second == key) return true;
    if (it->second == "+" && (key == "+" || key == "=")) return true;
  }
  return false;
}

} // namespace catalist
