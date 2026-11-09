#include <doctest/doctest.h>

#include "catalist/keybindManager.hpp"

using catalist::keyAction;
using catalist::keybindManager;

TEST_CASE("keybindManager plus binding also matches equals") {
  keybindManager keybinds;
  CHECK(keybinds.matches(keyAction::kIncrementProgress, "+"));
  CHECK(keybinds.matches(keyAction::kIncrementProgress, "="));
  CHECK_FALSE(keybinds.matches(keyAction::kDecrementProgress, "="));
}

TEST_CASE("keybindManager rebinding replaces the default key") {
  keybindManager keybinds;
  keybinds.setBinding("MoveDown", "n");
  CHECK(keybinds.matches(keyAction::kMoveDown, "n"));
  CHECK_FALSE(keybinds.matches(keyAction::kMoveDown, "j"));
  CHECK(keybinds.getKey(keyAction::kMoveDown) == "n");
}

TEST_CASE("keybindManager equals alias only applies to a plus binding") {
  keybindManager keybinds;
  keybinds.setBinding("increment", "i");
  CHECK(keybinds.matches(keyAction::kIncrementProgress, "i"));
  CHECK_FALSE(keybinds.matches(keyAction::kIncrementProgress, "="));
}

TEST_CASE("keybindManager ignores unknown action names") {
  keybindManager keybinds;
  keybinds.setBinding("doesNotExist", "z");
  CHECK(keybinds.getKey(keyAction::kQuit) == "q");
}
