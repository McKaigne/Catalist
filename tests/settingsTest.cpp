#include <doctest/doctest.h>

#include "catalist/settings/settings.hpp"
#include "support/testSupport.hpp"

using catalist::appSettings;
using catalist::test::tempDirFixture;
using catalist::test::writeTextFile;

TEST_CASE_FIXTURE(tempDirFixture, "settings default when the file is missing or broken") {
  auto missing = catalist::loadSettings(fileSystem(), tempDir() / "settings.json");
  CHECK(missing.theme == "catppuccin-mocha");
  CHECK(missing.mouse);
  CHECK_FALSE(missing.startupTab.has_value());

  writeTextFile(tempDir() / "settings.json", "{\"theme\": ");
  auto broken = catalist::loadSettings(fileSystem(), tempDir() / "settings.json");
  CHECK(broken.theme == "catppuccin-mocha");
}

TEST_CASE_FIXTURE(tempDirFixture, "settings round trip every field") {
  appSettings original;
  original.mouse = false;
  original.theme = "nord";
  original.defaultMedia = "manga";
  original.defaultSort = "title";
  original.reverseSort = false;
  original.startupTab = "stats";
  original.welcomeShown = true;
  REQUIRE(catalist::saveSettings(fileSystem(), tempDir() / "settings.json", original));

  auto loaded = catalist::loadSettings(fileSystem(), tempDir() / "settings.json");
  CHECK_FALSE(loaded.mouse);
  CHECK(loaded.theme == "nord");
  CHECK(loaded.defaultMedia == "manga");
  CHECK(loaded.defaultSort == "title");
  CHECK_FALSE(loaded.reverseSort);
  CHECK(loaded.startupTab == "stats");
  CHECK(loaded.welcomeShown);
}

TEST_CASE_FIXTURE(tempDirFixture, "settings ignore values of the wrong type or outside the choices") {
  writeTextFile(tempDir() / "settings.json",
                R"({"mouse": "no", "defaultMedia": "novels", "defaultSort": "title",
                    "reverseSort": 1, "theme": ""})");
  auto loaded = catalist::loadSettings(fileSystem(), tempDir() / "settings.json");
  CHECK(loaded.mouse);
  CHECK(loaded.defaultMedia == "anime");
  CHECK(loaded.defaultSort == "title");
  CHECK(loaded.reverseSort);
  CHECK(loaded.theme == "catppuccin-mocha");
}
