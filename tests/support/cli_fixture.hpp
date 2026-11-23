#ifndef CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_
#define CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_

#include <doctest/doctest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "cli/cli.hpp"
#include "support/test_support.hpp"

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
struct CliFixture : catalist::test::TempDirFixture {
	CliFixture() : previous_dir(std::filesystem::current_path()) {
		std::filesystem::current_path(TempDir());
	}
	~CliFixture() { std::filesystem::current_path(previous_dir); }

	// `input` is what the person types when asked; `columns` 0 is a pipe.
	int Run(const std::vector<std::string>& args, const std::string& input = "", int columns = 0) {
		catalist::Catalogue db("data");
		std::istringstream in_stream(input);
		std::ostringstream out_stream;
		std::ostringstream err_stream;
		if (out_closed)
			out_stream.setstate(std::ios::badbit);
		catalist::CliContext ctx{db, in_stream, out_stream, err_stream, TestClock, web.Link()};
		ctx.interactive = not input.empty();
		ctx.columns = columns;
		ctx.color = color;
		int exit_code = catalist::RunCli(args, ctx);
		out = out_stream.str();
		err = err_stream.str();
		return exit_code;
	}

	std::vector<catalist::Entry> Saved(MediaType type) {
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().has_value());
		return db.GetEntries(type);
	}

	catalist::Entry SavedTitle(MediaType type, const std::string& title) {
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().has_value());
		return catalist::test::FindTitle(db, type, title);
	}

	void Put(const std::string& path, const std::string& content) {
		catalist::test::WriteText(path, content);
	}

	// Four titles, two of them called Frieren (anime a:1 and manga m:2; Odd
	// Taxi is a:2 and Berserk m:1), with both their ids.
	void Seed() {
		catalist::Catalogue db("data");
		db.AddEntry(MakeEntry()
		                .Title("Sousou no Frieren")
		                .MalId(52991)
		                .AnilistId(154587)
		                .Status(WatchStatus::kCompleted)
		                .Score(10)
		                .Progress(28)
		                .TotalUnits(28));
		db.AddEntry(MakeEntry()
		                .Title("Odd Taxi")
		                .MalId(44265)
		                .AnilistId(128547)
		                .Status(WatchStatus::kWatching)
		                .Score(8)
		                .Progress(5)
		                .TotalUnits(13)
		                .StartDate("2026-10-01"));
		db.AddEntry(MakeEntry()
		                .Title("Berserk")
		                .Type(MediaType::kManga)
		                .MalId(2)
		                .AnilistId(30002)
		                .Status(WatchStatus::kWatching)
		                .Progress(364)
		                .StartDate("2019-05-01"));
		db.AddEntry(MakeEntry()
		                .Title("Sousou no Frieren")
		                .Type(MediaType::kManga)
		                .MalId(126287)
		                .AnilistId(118586)
		                .Status(WatchStatus::kPlanToWatch));
		REQUIRE(db.SaveAll());
	}

	// The steps of the undo history, newest first.
	std::vector<catalist::TxRecord> Steps() {
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().has_value());
		return db.History(0).value();
	}

	// A change no command made, as another program makes one.
	void ChangeBehindTheHistory(MediaType type, const std::string& title, int progress) {
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().has_value());
		auto changed = catalist::test::FindTitle(db, type, title);
		changed.progress = progress;
		REQUIRE(db.UpdateEntry(changed));
		REQUIRE(db.SaveAll());
	}

	catalist::test::FakeWeb web;
	std::filesystem::path previous_dir;
	// How the next runs are set up: colours, and a standard output whose
	// reader has gone.
	bool color = false;
	bool out_closed = false;
	std::string out;
	std::string err;
};

inline bool Has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_
