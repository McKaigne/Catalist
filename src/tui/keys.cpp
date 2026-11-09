#include "catalist/tui/keys.hpp"

#include <string_view>

namespace catalist::keys {

using ftxui::Event;

namespace {

std::string_view keyFor(keyAction action) {
  switch (action) {
    case keyAction::kMoveUp: return "k";
    case keyAction::kMoveDown: return "j";
    case keyAction::kJumpTop: return "g";
    case keyAction::kJumpBottom: return "G";
    case keyAction::kFocusLeft: return "h";
    case keyAction::kFocusRight: return "l";
    case keyAction::kIncrementProgress: return "+";
    case keyAction::kDecrementProgress: return "-";
    case keyAction::kEditNotes: return "e";
    case keyAction::kDeleteEntry: return "d";
    case keyAction::kSortDate: return "c";
    case keyAction::kSortTitle: return "t";
    case keyAction::kSortStatus: return "s";
    case keyAction::kSortRating: return "r";
    case keyAction::kSortProgress: return "p";
    case keyAction::kSearchFilter: return "/";
    case keyAction::kOpenBrowser: return "o";
    case keyAction::kYankTitle: return "y";
    case keyAction::kUndo: return "u";
    case keyAction::kRedo: return "U";
    case keyAction::kCommandPalette: return ":";
    case keyAction::kQuit: return "q";
    case keyAction::kToggleMedia: return "m";
  }
  return "";
}

}  // namespace

bool isBound(const Event& event, keyAction action) {
  if (!event.is_character())
    return false;
  std::string_view key = keyFor(action);
  // Unshifted '=' shares the '+' key on most layouts.
  return event.character() == key ||
         (key == "+" && event.character() == "=");
}

bool isMoveUp(const Event& event) {
  return event == Event::ArrowUp || isBound(event, keyAction::kMoveUp);
}

bool isMoveDown(const Event& event) {
  return event == Event::ArrowDown || isBound(event, keyAction::kMoveDown);
}

bool isMoveLeft(const Event& event) {
  return event == Event::ArrowLeft || isBound(event, keyAction::kFocusLeft);
}

bool isMoveRight(const Event& event) {
  return event == Event::ArrowRight || isBound(event, keyAction::kFocusRight);
}

bool isJumpTop(const Event& event) {
  return event == Event::Home || isBound(event, keyAction::kJumpTop);
}

bool isJumpBottom(const Event& event) {
  return event == Event::End || isBound(event, keyAction::kJumpBottom);
}

bool isConfirm(const Event& event) {
  return event == Event::Return;
}

bool isConfirmOrSpace(const Event& event) {
  return event == Event::Return || event == Event::Character(' ');
}

bool isCharacter(const Event& event, char expected) {
  return event == Event::Character(std::string(1, expected));
}

}  // namespace catalist::keys
