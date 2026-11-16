#include <doctest/doctest.h>

#include <ftxui/component/mouse.hpp>

#include "support/sampleLibrary.hpp"
#include "tui/app.hpp"
#include "tui/catalogTab.hpp"
#include "tui/tuiHarness.hpp"
#include "version.hpp"

using catalist::appShell;
using ftxui::Event;

namespace {

// Counts what the shell hands it, so routing can be checked.
class recordingTab : public catalist::iTab {
public:
	std::string title() const override { return "Probe"; }
	std::string icon() const override { return "?"; }
	ftxui::Element render(catalist::appContext&) override { return ftxui::text("probe body"); }
	bool onEvent(const Event&, catalist::appContext&) override {
		++events;
		return true;
	}
	std::vector<catalist::hint> hints() const override { return {{"x", "probe"}}; }
	void onActivate(catalist::appContext&) override { ++activations; }
	void onTick(catalist::appContext&) override { ++ticks; }

	int events = 0;
	int activations = 0;
	int ticks = 0;
};

struct seededHarness : catalist::test::tuiHarness {
	// Stored already, as the library the tracker opens is.
	seededHarness() {
		catalist::test::seedSampleLibrary(db);
		REQUIRE(db.saveAll());
	}
};

struct shellFixture : seededHarness {
	shellFixture()
	    : anime(catalist::mediaType::kAnime),
	      manga(catalist::mediaType::kManga),
	      shell(ctx, {&anime, &probe, &manga}, [this] { ++quits; }) {}

	std::string drawn() { return catalist::test::draw(shell.render()); }

	catalist::catalogTab anime;
	recordingTab probe;
	catalist::catalogTab manga;
	int quits = 0;
	appShell shell;
};

}  // namespace

TEST_CASE_FIXTURE(shellFixture, "tui shell switches tabs with numbers, Tab and Shift-Tab") {
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

TEST_CASE_FIXTURE(shellFixture, "tui shell quits on q unless the tab is taking text") {
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

TEST_CASE_FIXTURE(shellFixture, "tui shell passes other keys and ticks to the active tab only") {
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

TEST_CASE_FIXTURE(shellFixture, "tui shell bars show tabs, provider, status and hints") {
	auto shown = drawn();
	CHECK(shown.find("Anime (4)") != std::string::npos);
	CHECK(shown.find("Probe") != std::string::npos);
	CHECK(shown.find("Manga (2)") != std::string::npos);
	CHECK(shown.find(std::string("v") + catalist::kVersionString) != std::string::npos);
	CHECK(shown.find(" AniList ") != std::string::npos);
	CHECK(shown.find("Ready.") != std::string::npos);
	CHECK(shown.find("add") != std::string::npos);

	REQUIRE(shell.onEvent(Event::Character('/')));
	shown = drawn();
	CHECK(shown.find("Search title...") != std::string::npos);
	REQUIRE(shell.onEvent(Event::Escape));

	// The pill names whichever provider is chosen.
	db.setProvider(catalist::provider::kMyAnimeList);
	CHECK(drawn().find(" MAL ") != std::string::npos);
	db.setProvider(catalist::provider::kAniList);

	REQUIRE(shell.onEvent(Event::Character('2')));
	shown = drawn();
	CHECK(shown.find("probe body") != std::string::npos);
	CHECK(shown.find("probe") != std::string::npos);
}

TEST_CASE_FIXTURE(shellFixture, "tui shell fills in a finished manga's chapters after a change") {
	// Berserk was tracked while it ran, so its chapter total is unknown ("?").
	web.respond(R"({"id":2,"type":"MANGA"})", R"({"data":{"Media":{"id":30002,"idMal":2,
      "status":"FINISHED","chapters":380,"volumes":42}}})");
	REQUIRE(shell.onEvent(Event::Character('3')));
	REQUIRE(manga.shown().front().title == "Berserk");
	REQUIRE(shell.onEvent(Event::Character('+')));
	CHECK(drawn().find("365/?") != std::string::npos);

	updates.waitUntilIdle();
	REQUIRE(shell.onEvent(Event::Custom));
	auto berserk = catalist::test::findTitle(db, catalist::mediaType::kManga, "Berserk");
	CHECK(berserk.totalUnits == 380);
	CHECK(berserk.totalVolumes == 42);
	CHECK(berserk.released == 380);
	CHECK(catalist::formatDate(berserk.checkedAt) == "2026-10-20");
	CHECK(berserk.progress == 365);
	CHECK(berserk.anilistId == 30002);
	CHECK(ctx.status == "Updated from AniList: 'Berserk'.");
	auto shown = drawn();
	CHECK(shown.find("365/380") != std::string::npos);
	CHECK(shown.find("+15") != std::string::npos);
	CHECK(catalist::test::storedRows(dataDir(), catalist::mediaType::kManga)
	          .find("Berserk,Reading,10,365,380,42,42,0,2019-05-01,,2026-10-20,"
	                "380,2026-10-20,30002") != std::string::npos);

	// The chapter read is a step of the undo history; what the web filled in
	// is none, and stays when the step is taken back.
	auto history = db.history(0);
	REQUIRE(history.ok());
	REQUIRE(history.value().size() == 1);
	CHECK(history.value()[0].changes == 1);
	REQUIRE(db.undo(1, false).ok());
	berserk = catalist::test::findTitle(db, catalist::mediaType::kManga, "Berserk");
	CHECK(berserk.progress == 364);
	CHECK(berserk.totalUnits == 380);
	CHECK(berserk.released == 380);
}

TEST_CASE_FIXTURE(shellFixture,
                  "tui shell keeps the status line when a lookup changes nothing visible") {
	web.respond(R"({"id":44265,"type":"ANIME"})",
	            R"({"data":{"Media":{"status":"FINISHED","episodes":13}}})");
	updates.lookUp({catalist::mediaType::kAnime, 0, 44265});
	updates.waitUntilIdle();
	REQUIRE(shell.onEvent(Event::Custom));
	CHECK(ctx.status == "Ready.");
	CHECK(catalist::formatDate(
	          catalist::test::findTitle(db, catalist::mediaType::kAnime, "Odd Taxi").checkedAt) ==
	      "2026-10-20");
}

TEST_CASE_FIXTURE(shellFixture, "tui shell drops hints rather than crowd the status line") {
	using catalist::test::draw;
	using catalist::test::plain;
	ctx.status = "Sort: New releases (Decreasing ▼)";
	CHECK_FALSE(draw(shell.render(), 100, 40).empty());
	auto narrow = plain(draw(shell.render(), 100, 40));
	CHECK(narrow.find("Sort: New releases (Decreasing ▼)") != std::string::npos);
	CHECK(narrow.find("Tab tabs") != std::string::npos);
	CHECK(narrow.find("q quit") == std::string::npos);

	CHECK_FALSE(draw(shell.render(), 200, 40).empty());
	CHECK(plain(draw(shell.render(), 200, 40)).find("q quit") != std::string::npos);
}
