#include <doctest/doctest.h>

#include "catalist/config.hpp"
#include "catalist/keybindManager.hpp"
#include "support/testSupport.hpp"

using catalist::AppConfig;
using catalist::KeyAction;
using catalist::test::tempDirFixture;
using catalist::test::writeTextFile;

TEST_CASE_FIXTURE(tempDirFixture, "appConfig loads an old catalist.kdl with property style general") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath, R"kdl(general theme="nord" mouse=false default-media="manga" default-sort="title" reverse-sort=false

keybinds moveup="w" {
    movedown "s"
    move-up "x"
}

top-bar height=3 border=true {
    tab "anime" label="Anime"
    tab "sync" label="Sync"
    spacer
    text "v0.1.0" color="overlay0"
}
)kdl");

  AppConfig config;
  REQUIRE(config.load(configPath));
  CHECK(config.theme == "nord");
  CHECK(config.mouse == false);
  CHECK(config.defaultMedia == "manga");
  CHECK(config.defaultSort == "title");
  CHECK(config.reverseSort == false);
  CHECK(config.keybinds.getKey(KeyAction::MoveUp) == "w");
  CHECK(config.keybinds.getKey(KeyAction::MoveDown) == "s");

  auto topBar = config.kdlDoc.getNode("top-bar");
  REQUIRE(topBar.has_value());
  CHECK(topBar->children.back().getArgString(0) == "v0.1.0");
}

// Characterization: dashed keybind names (as written in the default config)
// do not match keybindManager's undashed action names and are ignored.
TEST_CASE_FIXTURE(tempDirFixture, "appConfig ignores dashed keybind names") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath, "keybinds {\n    move-up \"w\"\n}\n");
  AppConfig config;
  REQUIRE(config.load(configPath));
  CHECK(config.keybinds.getKey(KeyAction::MoveUp) == "k");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig loads child style general block") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath,
                "general {\n    theme \"dracula\"\n    mouse false\n"
                "    reverse-sort false\n}\n");
  AppConfig config;
  REQUIRE(config.load(configPath));
  CHECK(config.theme == "dracula");
  CHECK(config.mouse == false);
  CHECK(config.reverseSort == false);
  CHECK(config.defaultMedia == "anime");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig generates the default file when missing") {
  auto configPath = tempDir() / "nested" / "catalist.kdl";
  AppConfig config;
  REQUIRE(config.load(configPath));
  CHECK(std::filesystem::exists(configPath));
  CHECK(config.kdlDoc.getNode("top-bar").has_value());
  CHECK(config.theme == "catppuccin-mocha");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig save then load keeps general settings") {
  auto configPath = tempDir() / "catalist.kdl";
  AppConfig original;
  original.theme = "gruvbox";
  original.mouse = false;
  original.defaultSort = "progress";
  REQUIRE(original.save(configPath));

  AppConfig reloaded;
  REQUIRE(reloaded.load(configPath));
  CHECK(reloaded.theme == "gruvbox");
  CHECK(reloaded.mouse == false);
  CHECK(reloaded.defaultSort == "progress");
}
