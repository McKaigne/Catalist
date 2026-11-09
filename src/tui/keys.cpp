#include "catalist/tui/keys.hpp"

namespace catalist::keys {

using ftxui::Event;

bool isBound(const Event& event, const keybindManager& keybinds,
             keyAction action) {
  return event.is_character() && keybinds.matches(action, event.character());
}

bool isMoveUp(const Event& event, const keybindManager& keybinds) {
  return event == Event::ArrowUp || isBound(event, keybinds, keyAction::kMoveUp);
}

bool isMoveDown(const Event& event, const keybindManager& keybinds) {
  return event == Event::ArrowDown ||
         isBound(event, keybinds, keyAction::kMoveDown);
}

bool isMoveLeft(const Event& event, const keybindManager& keybinds) {
  return event == Event::ArrowLeft ||
         isBound(event, keybinds, keyAction::kFocusLeft);
}

bool isMoveRight(const Event& event, const keybindManager& keybinds) {
  return event == Event::ArrowRight ||
         isBound(event, keybinds, keyAction::kFocusRight);
}

bool isJumpTop(const Event& event, const keybindManager& keybinds) {
  return event == Event::Home || isBound(event, keybinds, keyAction::kJumpTop);
}

bool isJumpBottom(const Event& event, const keybindManager& keybinds) {
  return event == Event::End || isBound(event, keybinds, keyAction::kJumpBottom);
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
