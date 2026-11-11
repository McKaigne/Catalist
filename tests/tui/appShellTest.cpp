#include <doctest/doctest.h>

#include <ftxui/component/mouse.hpp>

#include "version.hpp"
#include "tui/app.hpp"
#include "tui/catalogTab.hpp"
#include "tui/statsTab.hpp"
#include "support/sampleLibrary.hpp"
#include "tui/tuiHarness.hpp"

using catalist::appShell;
using ftxui::Event;

namespace {

// Counts what the shell hands it, so routing can be checked.
class recordingTab : public catalist::iTab {
public:
  std::string title() const override { return "Probe"; }
  std::string icon() const override { return "?"; }
  ftxui::Element render(catalist::appContext&) override {
    return ftxui::text("probe body");
  }
  bool onEvent(const Event&, catalist::appContext&) override {
    ++events;
    return true;
  }
  std::vector<catalist::hint> hints() const override {
    return {{"x", "probe"}};
  }
  void onActivate(catalist::appContext&) override { ++activations; }
  void onTick(catalist::appContext&) override { ++ticks; }

  int events = 0;
  int activations = 0;
  int ticks = 0;
};

struct seededHarness : catalist::test::tuiHarness {
  seededHarness() { catalist::test::seedSampleLibrary(db); }
};

struct shellFixture : seededHarness {
  shellFixture()
      : anime(catalist::mediaType::kAnime),
        shell(ctx, {&anime, &probe, &stats}, [this] { ++quits; }) {}

  std::string drawn() { return catalist::test::draw(shell.render()); }

  catalist::catalogTab anime;
  recordingTab probe;
  catalist::statsTab stats;
  int quits = 0;
  appShell shell;
};

}  // namespace

TEST_CASE_FIXTURE(shellFixture,
                  "tui shell switches tabs with numbers, Tab and Shift-Tab") {
  CHECK(shell.activeIndex() == 0);
  REQUIRE(shell.onEvent(Event::Character('2')));
  CHECK(shell.activeIndex() == 1);
  CHECK(probe.activations == 1);
  REQUIRE(shell.onEvent(Event::Tab));
  CHECK(shell.activeIndex() == 2);
  REQUIRE(shell.onEvent(Event::Tab));
  CHECK(shell.activeIndex() == 0);
  REQUIRE(shell.onEvent(Event::TabReverse));
  CHECK(shell.activeIndex() == 2);
  REQUIRE(shell.onEvent(Event::Character('1')));
  CHECK(shell.activeIndex() == 0);
  CHECK(probe.events == 0);
}

TEST_CASE_FIXTURE(shellFixture,
                  "tui shell quits on q unless the tab is taking text") {
  REQUIRE(shell.onEvent(Event::Character('/')));
  REQUIRE(shell.onEvent(Event::Character('q')));
  REQUIRE(shell.onEvent(Event::Character('2')));
  CHECK(quits == 0);
  CHECK(shell.activeIndex() == 0);
  CHECK(anime.shown().empty());
  CHECK(drawn().find("Filter: 'q2'") != std::string::npos);

  REQUIRE(shell.onEvent(Event::Escape));
  REQUIRE(shell.onEvent(Event::Character('q')));
  CHECK(quits == 1);
}

TEST_CASE_FIXTURE(
    shellFixture,
    "tui shell passes other keys and ticks to the active tab only") {
  REQUIRE(shell.onEvent(Event::Character('2')));
  REQUIRE(shell.onEvent(Event::Character('x')));
  REQUIRE(shell.onEvent(Event::Custom));
  CHECK(probe.events == 1);
  CHECK(probe.ticks == 1);
  REQUIRE(shell.onEvent(Event::Character('1')));
  REQUIRE(shell.onEvent(Event::Custom));
  CHECK(probe.ticks == 1);
}

TEST_CASE_FIXTURE(shellFixture, "tui shell ignores the mouse") {
  REQUIRE(shell.onEvent(Event::Character('2')));
  ftxui::Mouse click;
  click.button = ftxui::Mouse::Left;
  click.motion = ftxui::Mouse::Pressed;
  CHECK(shell.onEvent(Event::Mouse("", click)));
  CHECK(probe.events == 0);
}

TEST_CASE_FIXTURE(shellFixture,
                  "tui shell bars show tabs, mode, status and hints") {
  auto shown = drawn();
  CHECK(shown.find("Anime (4)") != std::string::npos);
  CHECK(shown.find("Probe") != std::string::npos);
  CHECK(shown.find("Stats") != std::string::npos);
  CHECK(shown.find(std::string("v") + catalist::kVersionString) !=
        std::string::npos);
  CHECK(shown.find(" NOR ") != std::string::npos);
  CHECK(shown.find("Ready.") != std::string::npos);
  CHECK(shown.find("add") != std::string::npos);

  REQUIRE(shell.onEvent(Event::Character('/')));
  shown = drawn();
  CHECK(shown.find(" SCH ") != std::string::npos);
  CHECK(shown.find("Search title...") != std::string::npos);
  REQUIRE(shell.onEvent(Event::Escape));

  REQUIRE(shell.onEvent(Event::Character('a')));
  CHECK(drawn().find(" ADD ") != std::string::npos);
  REQUIRE(shell.onEvent(Event::Escape));

  REQUIRE(shell.onEvent(Event::Character('d')));
  CHECK(drawn().find(" DEL ") != std::string::npos);
  REQUIRE(shell.onEvent(Event::Character('n')));

  REQUIRE(shell.onEvent(Event::Character('2')));
  shown = drawn();
  CHECK(shown.find("probe body") != std::string::npos);
  CHECK(shown.find("probe") != std::string::npos);
}
