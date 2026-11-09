#include <doctest/doctest.h>

#include <memory>

#include "catalist/tui/app.hpp"
#include "catalist/tui/tabRegistry.hpp"
#include "catalist/tui/tabs/settingsTab.hpp"
#include "catalist/tui/tabs/statsTab.hpp"

namespace {

catalist::tabRegistry twoTabs() {
  catalist::tabRegistry registry;
  registry.add(std::make_unique<catalist::statsTab>());
  registry.add(std::make_unique<catalist::settingsTab>(
      std::vector<std::string>{"stats", "settings"}));
  return registry;
}

catalist::kdl::document parsed(const std::string& text) {
  std::string err;
  return catalist::kdl::document::parse(text, err);
}

std::vector<std::string> ids(const std::vector<catalist::iTab*>& tabs) {
  std::vector<std::string> listed;
  for (auto* tab : tabs)
    listed.push_back(tab->id());
  return listed;
}

}  // namespace

TEST_CASE("tui tabRegistry orders tabs by the top-bar node") {
  auto registry = twoTabs();
  auto config = parsed("top-bar {\n tab \"settings\"\n tab \"nope\"\n tab \"stats\"\n"
                       " tab \"settings\"\n}\n");
  CHECK(ids(registry.ordered(config)) ==
        std::vector<std::string>{"settings", "stats"});
}

TEST_CASE("tui tabRegistry falls back to registration order") {
  auto registry = twoTabs();
  CHECK(ids(registry.ordered(parsed(""))) ==
        std::vector<std::string>{"stats", "settings"});
  CHECK(registry.find("settings") != nullptr);
  CHECK(registry.find("about") == nullptr);
}

TEST_CASE("tui built in tab ids match the documented order") {
  CHECK(catalist::builtInTabIds() ==
        std::vector<std::string>{"anime", "manga", "stats", "settings"});
}
