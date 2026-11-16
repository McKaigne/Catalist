#ifndef CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_
#define CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_

#include <doctest/doctest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "cli/cli.hpp"
#include "support/testSupport.hpp"

// What the tests of the command line share: a run of `catalist ...` against
// a temporary directory, the lists it starts from, and two exports.
namespace catalist::test {

inline const char* const kAnimeXml =
    "<myanimelist>"
    "<anime><series_animedb_id>52991</series_animedb_id>"
    "<series_title><![CDATA[Sousou no Frieren]]></series_title>"
    "<series_episodes>28</series_episodes>"
    "<my_watched_episodes>28</my_watched_episodes><my_score>10</my_score>"
    "<my_status>Completed</my_status></anime>"
    "<anime><series_animedb_id>44265</series_animedb_id>"
    "<series_title><![CDATA[Odd Taxi]]></series_title>"
    "<series_episodes>13</series_episodes>"
    "<my_watched_episodes>5</my_watched_episodes><my_score>8</my_score>"
    "<my_status>Watching</my_status></anime>"
    "</myanimelist>";

inline const char* const kMangaXml =
    "<myanimelist>"
    "<manga><manga_mangadb_id>2</manga_mangadb_id>"
    "<manga_title><![CDATA[Berserk]]></manga_title>"
    "<my_read_chapters>364</my_read_chapters><my_score>10</my_score>"
    "<my_status>Reading</my_status></manga>"
    "</myanimelist>";

// One `catalist ...` run with the data in data/ of a temporary directory,
// which is the working directory while the test runs, so file names in the
// output stay short. Each run gets a fresh catalogue, as each real
// invocation does; AniList is a fake that knows only what a test tells it.
struct cliFixture : catalist::test::tempDirFixture {
	cliFixture() : previousDir(std::filesystem::current_path()) {
		std::filesystem::current_path(tempDir());
	}
	~cliFixture() { std::filesystem::current_path(previousDir); }

	// `input` is what the person types when asked; `columns` 0 is a pipe.
	int run(const std::vector<std::string>& args, const std::string& input = "", int columns = 0) {
		catalist::catalogue db("data");
		std::istringstream inStream(input);
		std::ostringstream outStream;
		std::ostringstream errStream;
		if (outClosed)
			outStream.setstate(std::ios::badbit);
		catalist::cliContext ctx{db, inStream, outStream, errStream, testToday, web.link()};
		ctx.interactive = not input.empty();
		ctx.columns = columns;
		ctx.color = color;
		ctx.program = program;
		ctx.nowStamp = [] { return std::string(kStamp); };
		ctx.newTxLabel = [] { return std::string("1792531800-4242"); };
		ctx.txLabel = group;
		ctx.accountsFile = "state/accounts";
		ctx.net.ids = ids;
		ctx.net.now = [this] { return clock; };
		int exitCode = catalist::runCli(args, ctx);
		out = outStream.str();
		err = errStream.str();
		return exitCode;
	}

	std::vector<catalist::entry> saved(mediaType type) {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		return db.getEntries(type);
	}

	catalist::entry savedTitle(mediaType type, const std::string& title) {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		return catalist::test::findTitle(db, type, title);
	}

	void put(const std::string& path, const std::string& content) {
		catalist::test::writeText(path, content);
	}

	// Four titles, two of them called Frieren (anime a:1 and manga m:2; Odd
	// Taxi is a:2 and Berserk m:1), with both their ids.
	void seed() {
		catalist::catalogue db("data");
		db.addEntry(makeEntry()
		                .title("Sousou no Frieren")
		                .malId(52991)
		                .anilistId(154587)
		                .status(watchStatus::kCompleted)
		                .score(10)
		                .progress(28)
		                .totalUnits(28)
		                .checkedAt("2026-10-20"));
		db.addEntry(makeEntry()
		                .title("Odd Taxi")
		                .malId(44265)
		                .anilistId(128547)
		                .status(watchStatus::kWatching)
		                .score(8)
		                .progress(5)
		                .totalUnits(13)
		                .released(13)
		                .startDate("2026-10-01")
		                .checkedAt("2026-10-20"));
		db.addEntry(makeEntry()
		                .title("Berserk")
		                .type(mediaType::kManga)
		                .malId(2)
		                .anilistId(30002)
		                .status(watchStatus::kWatching)
		                .progress(364)
		                .startDate("2019-05-01")
		                .checkedAt("2026-10-20"));
		db.addEntry(makeEntry()
		                .title("Sousou no Frieren")
		                .type(mediaType::kManga)
		                .malId(126287)
		                .anilistId(118586)
		                .status(watchStatus::kPlanToWatch)
		                .checkedAt("2026-10-20"));
		REQUIRE(db.saveAll());
		REQUIRE(db.saveSettings());
	}

	// The steps of the undo history, newest first.
	std::vector<catalist::txRecord> steps() {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		return db.history(0).value();
	}

	// A change no command made, as another program (Catalist-TUI's web
	// check) makes one.
	void changeBehindTheHistory(mediaType type, const std::string& title, int progress) {
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().ok());
		auto changed = catalist::test::findTitle(db, type, title);
		changed.progress = progress;
		REQUIRE(db.updateEntry(changed));
		REQUIRE(db.saveAll());
	}

	// When every change of a test is made.
	static constexpr const char* kStamp = "2026-10-20 21:30:00";

	catalist::test::fakeWeb web;
	std::filesystem::path previousDir;
	// How the next runs are set up: the name the program goes by, colours,
	// a standard output whose reader has gone, and CATALIST_TX.
	std::string program = "catalist";
	bool color = false;
	bool outClosed = false;
	std::string group;
	// The ids the sites gave this Catalist, and the time, in seconds.
	catalist::web::appIds ids;
	long long clock = 1792531800;
	std::string out;
	std::string err;
};

inline bool has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_
