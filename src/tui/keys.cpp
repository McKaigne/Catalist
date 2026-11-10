#include "catalist/tui/keys.hpp"

#include <string>

namespace catalist::keys {

using ftxui::Event;

bool isMoveUp(const Event& event) {
  return event == Event::ArrowUp || isCharacter(event, 'k');
}

bool isMoveDown(const Event& event) {
  return event == Event::ArrowDown || isCharacter(event, 'j');
}

bool isMoveLeft(const Event& event) {
  return event == Event::ArrowLeft || isCharacter(event, 'h');
}

bool isMoveRight(const Event& event) {
  return event == Event::ArrowRight || isCharacter(event, 'l');
}

bool isJumpTop(const Event& event) {
  return event == Event::Home || isCharacter(event, 'g');
}

bool isJumpBottom(const Event& event) {
  return event == Event::End || isCharacter(event, 'G');
}

bool isConfirm(const Event& event) {
  return event == Event::Return;
}

bool isConfirmOrSpace(const Event& event) {
  return event == Event::Return || event == Event::Character(' ');
}

bool isIncrement(const Event& event) {
  return isCharacter(event, '+') || isCharacter(event, '=');
}

bool isCharacter(const Event& event, char expected) {
  return event == Event::Character(std::string(1, expected));
}

}  // namespace catalist::keys
