#include <doctest/doctest.h>

#include <algorithm>

#include "catalist/config/keybindManager.hpp"

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

TEST_CASE("keybindManager lists every action with its current key") {
  catalist::keybindManager keybinds;
  keybinds.setBinding("moveitemdown", "n");
  auto listed = keybinds.bindings();
  CHECK(listed.size() == 36);
  auto reorder = std::find_if(listed.begin(), listed.end(), [](const auto& b) {
    return b.action == catalist::keyAction::kMoveItemDown;
  });
  REQUIRE(reorder != listed.end());
  CHECK(reorder->name == "moveitemdown");
  CHECK(reorder->key == "n");
}

TEST_CASE("keybindManager accepts every legacy alias name") {
  catalist::keybindManager keybinds;
  keybinds.setBinding("CyclePane", "x");
  keybinds.setBinding("yank", "Y");
  keybinds.setBinding("help", "H");
  CHECK(keybinds.getKey(catalist::keyAction::kCyclePaneNext) == "x");
  CHECK(keybinds.getKey(catalist::keyAction::kYankTitle) == "Y");
  CHECK(keybinds.getKey(catalist::keyAction::kOpenHelp) == "H");
}
