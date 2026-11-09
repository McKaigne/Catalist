#include <doctest/doctest.h>

#include "catalist/keybindManager.hpp"

using catalist::keyAction;
using catalist::keybindManager;

TEST_CASE("keybindManager plus binding also matches equals") {
  keybindManager keybinds;
  CHECK(keybinds.matches(keyAction::IncrementProgress, "+"));
  CHECK(keybinds.matches(keyAction::IncrementProgress, "="));
  CHECK_FALSE(keybinds.matches(keyAction::DecrementProgress, "="));
}

TEST_CASE("keybindManager rebinding replaces the default key") {
  keybindManager keybinds;
  keybinds.setBinding("MoveDown", "n");
  CHECK(keybinds.matches(keyAction::MoveDown, "n"));
  CHECK_FALSE(keybinds.matches(keyAction::MoveDown, "j"));
  CHECK(keybinds.getKey(keyAction::MoveDown) == "n");
}

TEST_CASE("keybindManager equals alias only applies to a plus binding") {
  keybindManager keybinds;
  keybinds.setBinding("increment", "i");
  CHECK(keybinds.matches(keyAction::IncrementProgress, "i"));
  CHECK_FALSE(keybinds.matches(keyAction::IncrementProgress, "="));
}

TEST_CASE("keybindManager ignores unknown action names") {
  keybindManager keybinds;
  keybinds.setBinding("doesNotExist", "z");
  CHECK(keybinds.getKey(keyAction::Quit) == "q");
}
