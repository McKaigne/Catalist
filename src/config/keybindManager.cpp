#include "catalist/config/keybindManager.hpp"

#include <algorithm>
#include <cctype>

namespace catalist {

namespace {

struct actionSpec {
  keyAction action;
  const char* defaultKey;
  // Lower-case names accepted in catalist.kdl; the first is canonical.
  std::vector<const char*> names;
};

const std::vector<actionSpec>& actionTable() {
  static const std::vector<actionSpec> table = {
      {keyAction::kMoveUp, "k", {"moveup"}},
      {keyAction::kMoveDown, "j", {"movedown"}},
      {keyAction::kJumpTop, "g", {"jumptop"}},
      {keyAction::kJumpBottom, "G", {"jumpbottom"}},
      {keyAction::kFocusLeft, "h", {"focusleft"}},
      {keyAction::kFocusRight, "l", {"focusright"}},
      {keyAction::kCyclePaneNext, "Tab", {"cyclepanenext", "cyclepane"}},
      {keyAction::kCyclePanePrev, "BackTab", {"cyclepaneprev"}},
      {keyAction::kIncrementProgress, "+", {"increment", "incrementprogress"}},
      {keyAction::kDecrementProgress, "-", {"decrement", "decrementprogress"}},
      {keyAction::kCycleStatus, "Space", {"cyclestatus"}},
      {keyAction::kTabAnime, "1", {"tabanime"}},
      {keyAction::kTabManga, "2", {"tabmanga"}},
      {keyAction::kTabOverview, "3", {"taboverview"}},
      {keyAction::kInspectDetails, "Enter", {"inspect", "inspectdetails"}},
      {keyAction::kEditNotes, "e", {"editnotes"}},
      {keyAction::kRenameTitle, "t", {"renametitle", "rename"}},
      {keyAction::kAddEntry, "a", {"add", "addentry"}},
      {keyAction::kDeleteEntry, "d", {"delete", "deleteentry"}},
      {keyAction::kSortDate, "c", {"sortdate"}},
      {keyAction::kSortTitle, "t", {"sorttitle"}},
      {keyAction::kSortStatus, "s", {"sortstatus"}},
      {keyAction::kSortRating, "r", {"sortrating"}},
      {keyAction::kSortProgress, "p", {"sortprogress"}},
      {keyAction::kSearchFilter, "/", {"search", "searchfilter"}},
      {keyAction::kOpenBrowser, "o", {"openbrowser"}},
      {keyAction::kYankTitle, "y", {"yank", "yanktitle"}},
      {keyAction::kUndo, "u", {"undo"}},
      {keyAction::kRedo, "U", {"redo"}},
      {keyAction::kCommandPalette, ":", {"commandpalette"}},
      {keyAction::kOpenHelp, "?", {"openhelp", "help"}},
      {keyAction::kQuit, "q", {"quit"}},
      {keyAction::kMoveItemUp, "K", {"moveitemup"}},
      {keyAction::kMoveItemDown, "J", {"moveitemdown"}},
      {keyAction::kToggleMedia, "m", {"togglemedia"}},
      {keyAction::kNextProvider, "p", {"nextprovider"}},
  };
  return table;
}

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

}  // namespace

keybindManager::keybindManager() {
  for (const auto& spec : actionTable())
    primaryBindings_[spec.action] = spec.defaultKey;
}

void keybindManager::setBinding(const std::string& actionName,
                                const std::string& key) {
  std::string wanted = lowercase(actionName);
  for (const auto& spec : actionTable()) {
    bool named = std::any_of(spec.names.begin(), spec.names.end(),
                             [&](const char* name) { return wanted == name; });
    if (named) {
      primaryBindings_[spec.action] = key;
      return;
    }
  }
}

std::string keybindManager::getKey(keyAction action) const {
  auto found = primaryBindings_.find(action);
  return found == primaryBindings_.end() ? "" : found->second;
}

bool keybindManager::matches(keyAction action, const std::string& key) const {
  std::string bound = getKey(action);
  if (bound.empty())
    return false;
  // Unshifted '=' shares the '+' key on most layouts.
  return bound == key || (bound == "+" && key == "=");
}

std::vector<keyBinding> keybindManager::bindings() const {
  std::vector<keyBinding> listed;
  for (const auto& spec : actionTable())
    listed.push_back({spec.action, *spec.names.begin(), getKey(spec.action)});
  return listed;
}

}  // namespace catalist
