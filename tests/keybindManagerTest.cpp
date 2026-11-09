#include <doctest/doctest.h>

#include "catalist/keybindManager.hpp"

using catalist::KeyAction;
using catalist::KeybindManager;

TEST_CASE("keybindManager plus binding also matches equals") {
  KeybindManager keybinds;
  CHECK(keybinds.matches(KeyAction::IncrementProgress, "+"));
  CHECK(keybinds.matches(KeyAction::IncrementProgress, "="));
  CHECK_FALSE(keybinds.matches(KeyAction::DecrementProgress, "="));
}

TEST_CASE("keybindManager rebinding replaces the default key") {
  KeybindManager keybinds;
  keybinds.setBinding("MoveDown", "n");
  CHECK(keybinds.matches(KeyAction::MoveDown, "n"));
  CHECK_FALSE(keybinds.matches(KeyAction::MoveDown, "j"));
  CHECK(keybinds.getKey(KeyAction::MoveDown) == "n");
}

TEST_CASE("keybindManager equals alias only applies to a plus binding") {
  KeybindManager keybinds;
  keybinds.setBinding("increment", "i");
  CHECK(keybinds.matches(KeyAction::IncrementProgress, "i"));
  CHECK_FALSE(keybinds.matches(KeyAction::IncrementProgress, "="));
}

TEST_CASE("keybindManager ignores unknown action names") {
  KeybindManager keybinds;
  keybinds.setBinding("doesNotExist", "z");
  CHECK(keybinds.getKey(KeyAction::Quit) == "q");
}
