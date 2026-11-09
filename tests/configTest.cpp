#include <doctest/doctest.h>

#include "catalist/config/config.hpp"
#include "catalist/config/keybindManager.hpp"
#include "support/testSupport.hpp"

using catalist::appConfig;
using catalist::keyAction;
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

  appConfig config;
  REQUIRE(config.load(fileSystem(), configPath));
  CHECK(config.theme == "nord");
  CHECK(config.mouse == false);
  CHECK(config.defaultMedia == "manga");
  CHECK(config.defaultSort == "title");
  CHECK(config.reverseSort == false);
  CHECK(config.keybinds.getKey(keyAction::kMoveUp) == "w");
  CHECK(config.keybinds.getKey(keyAction::kMoveDown) == "s");

  auto topBar = config.kdlDoc.getNode("top-bar");
  REQUIRE(topBar.has_value());
  CHECK(topBar->children.back().name == "version");
  CHECK(topBar->children.back().args.empty());
  CHECK(topBar->children.back().getPropString("color") == "overlay0");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig keeps top-bar text that is not a version label") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath, "top-bar {\n    text \"v1.2\"\n    text \"hello\"\n"
                            "    text \"v1.2.x\"\n}\n");
  appConfig config;
  REQUIRE(config.load(fileSystem(), configPath));
  auto topBar = config.kdlDoc.getNode("top-bar");
  REQUIRE(topBar.has_value());
  REQUIRE(topBar->children.size() == 3);
  for (const auto& child : topBar->children)
    CHECK(child.name == "text");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig default top bar uses a version node") {
  appConfig config;
  REQUIRE(config.load(fileSystem(), tempDir() / "catalist.kdl"));
  auto topBar = config.kdlDoc.getNode("top-bar");
  REQUIRE(topBar.has_value());
  bool hasVersionNode = false;
  for (const auto& child : topBar->children)
    hasVersionNode = hasVersionNode || child.name == "version";
  CHECK(hasVersionNode);
}

// Characterization: dashed keybind names (as written in the default config)
// do not match keybindManager's undashed action names and are ignored.
TEST_CASE_FIXTURE(tempDirFixture, "appConfig ignores dashed keybind names") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath, "keybinds {\n    move-up \"w\"\n}\n");
  appConfig config;
  REQUIRE(config.load(fileSystem(), configPath));
  CHECK(config.keybinds.getKey(keyAction::kMoveUp) == "k");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig loads child style general block") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath,
                "general {\n    theme \"dracula\"\n    mouse false\n"
                "    reverse-sort false\n}\n");
  appConfig config;
  REQUIRE(config.load(fileSystem(), configPath));
  CHECK(config.theme == "dracula");
  CHECK(config.mouse == false);
  CHECK(config.reverseSort == false);
  CHECK(config.defaultMedia == "anime");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig generates the default file when missing") {
  auto configPath = tempDir() / "nested" / "catalist.kdl";
  appConfig config;
  REQUIRE(config.load(fileSystem(), configPath));
  CHECK(std::filesystem::exists(configPath));
  CHECK(config.kdlDoc.getNode("top-bar").has_value());
  CHECK(config.theme == "catppuccin-mocha");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig save then load keeps general settings") {
  auto configPath = tempDir() / "catalist.kdl";
  appConfig original;
  original.theme = "gruvbox";
  original.mouse = false;
  original.defaultSort = "progress";
  REQUIRE(original.save(fileSystem(), configPath));

  appConfig reloaded;
  REQUIRE(reloaded.load(fileSystem(), configPath));
  CHECK(reloaded.theme == "gruvbox");
  CHECK(reloaded.mouse == false);
  CHECK(reloaded.defaultSort == "progress");
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig upgrades a pre 0.3 top bar to the new tab set") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath, R"kdl(top-bar height=3 {
    tab "about" label="Catalist"
    tab "anime" label="Anime" show-count=true
    tab "sync" label="Sync"
    spacer
    text "v0.1.9" color="overlay0"
}
)kdl");
  appConfig config;
  REQUIRE(config.load(fileSystem(), configPath));
  auto topBar = config.kdlDoc.getNode("top-bar");
  REQUIRE(topBar.has_value());
  std::vector<std::string> ids;
  for (const auto& child : topBar->children) {
    if (child.name == "tab")
      ids.push_back(child.getArgString(0));
  }
  CHECK(ids == std::vector<std::string>{"anime", "manga", "queue", "search",
                                        "library", "history", "stats",
                                        "settings", "help"});
  CHECK(topBar->children[9].name == "spacer");
  CHECK(topBar->children[10].name == "version");
  CHECK(topBar->getPropInt("height") == 3);
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig keeps a top bar that already uses current tab ids") {
  auto configPath = tempDir() / "catalist.kdl";
  writeTextFile(configPath,
                "top-bar {\n    tab \"manga\"\n    tab \"queue\"\n}\n");
  appConfig config;
  REQUIRE(config.load(fileSystem(), configPath));
  CHECK(config.kdlDoc.getNode("top-bar")->children.size() == 2);
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig startup tab and welcome flag round trip") {
  auto configPath = tempDir() / "catalist.kdl";
  appConfig original;
  CHECK_FALSE(original.startupTab.has_value());
  original.startupTab = "queue";
  original.welcomeShown = true;
  REQUIRE(original.save(fileSystem(), configPath));

  appConfig reloaded;
  REQUIRE(reloaded.load(fileSystem(), configPath));
  CHECK(reloaded.startupTab == "queue");
  CHECK(reloaded.welcomeShown);
}

TEST_CASE_FIXTURE(tempDirFixture, "appConfig default file lists the new tabs and a startup tab") {
  appConfig config;
  REQUIRE(config.load(fileSystem(), tempDir() / "fresh.kdl"));
  auto general = config.kdlDoc.getNode("general");
  REQUIRE(general.has_value());
  CHECK(general->getChild("startup-tab")->getArgString(0) == "anime");
  auto topBar = config.kdlDoc.getNode("top-bar");
  REQUIRE(topBar.has_value());
  CHECK(topBar->children.front().getArgString(0) == "anime");
}
