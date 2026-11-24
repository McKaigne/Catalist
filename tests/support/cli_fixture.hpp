#ifndef CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_
#define CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <functional>
#include <sstream>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

#include "cli/cli.hpp"
#include "support/test_support.hpp"

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

// Standard input that does `meanwhile` when it is first read, then gives the
// answer.
class AnswerAfter : public std::streambuf {
public:
	AnswerAfter(std::string answer, std::function<void()> meanwhile)
	    : answer_(std::move(answer)), meanwhile_(std::move(meanwhile)) {}

protected:
	int_type underflow() override {
		if (meanwhile_) {
			std::exchange(meanwhile_, nullptr)();
			setg(answer_.data(), answer_.data(), answer_.data() + answer_.size());
		}
		return gptr() < egptr() ? traits_type::to_int_type(*gptr()) : traits_type::eof();
	}

private:
	std::string answer_;
	std::function<void()> meanwhile_;
};

struct CliFixture : catalist::test::TempDirFixture {
	CliFixture() : previous_dir(std::filesystem::current_path()) {
		std::filesystem::current_path(TempDir());
	}
	~CliFixture() { std::filesystem::current_path(previous_dir); }

	int Run(const std::vector<std::string>& args, const std::string& input = "", int columns = 0) {
		std::istringstream in_stream(input);
		return RunOn(args, in_stream, not input.empty(), columns);
	}

	int RunAsked(const std::vector<std::string>& args, const std::string& answer,
	             std::function<void()> meanwhile) {
		AnswerAfter typed(answer, std::move(meanwhile));
		std::istream in_stream(&typed);
		return RunOn(args, in_stream, true, 0);
	}

	int RunOn(const std::vector<std::string>& args, std::istream& in_stream, bool interactive,
	          int columns) {
		catalist::Catalogue db("data");
		std::ostringstream out_stream;
		std::ostringstream err_stream;
		if (out_closed)
			out_stream.setstate(std::ios::badbit);
		catalist::CliContext ctx{db, in_stream, out_stream, err_stream, TestClock, web.Link()};
		ctx.interactive = interactive;
		ctx.columns = columns;
		ctx.rows = rows;
		ctx.pager = pager;
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

	void KnowTitle(const std::string& name, int id) {
		web.Respond(std::format(R"("search":"{}")", name),
		            std::format(R"({{"data":{{"anime":{{"media":[{{"id":{0},"idMal":{0},)"
		                        R"("title":{{"romaji":"{1}"}}}}]}},"manga":{{"media":[]}}}}}})",
		                        id, name));
	}

	void Put(const std::string& path, const std::string& content) {
		catalist::test::WriteText(path, content);
	}

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

	std::vector<catalist::TxRecord> Steps() {
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().has_value());
		return db.History(0).value();
	}

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
	bool color = false;
	bool out_closed = false;
	int rows = 0;
	std::function<bool(const std::string&)> pager;
	std::string out;
	std::string err;
};

inline bool Has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_CLI_FIXTURE_HPP_
