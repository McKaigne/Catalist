#ifndef CATALIST_TESTS_SUPPORT_MENU_FIXTURE_HPP_
#define CATALIST_TESTS_SUPPORT_MENU_FIXTURE_HPP_

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <sstream>
#include <string>
#include <vector>

#include "menu/app.hpp"
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

inline std::string Plain(const std::string& text) {
	std::string plain;
	for (size_t i = 0; i < text.size();) {
		auto byte = static_cast<unsigned char>(text[i]);
		if (byte < 0xEE or (byte > 0xEF and byte != 0xF3)) {
			plain += text[i++];
			continue;
		}
		size_t length = byte == 0xF3 ? 4 : 3;
		std::uint32_t point = byte & (byte == 0xF3 ? 0x07 : 0x0F);
		for (size_t k = 1; k < length; ++k)
			point = (point << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
		i += length;
		bool icon = (point >= 0xE000 and point <= 0xF8FF) or point >= 0xF0000;
		if (not icon) {
			plain += text.substr(i - length, length);
		} else if (point >= 0xF0C9E and point <= 0xF0CB0 and (point - 0xF0C9E) % 2 == 0) {
			plain += static_cast<char>('0' + (point - 0xF0C9E) / 2);
		} else if (point >= 0xF0BEC and point <= 0xF0C5F and (point - 0xF0BEC) % 3 == 0) {
			plain += static_cast<char>('a' + (point - 0xF0BEC) / 3);
		} else if (point == 0xF0CDF) {
			plain += "->";
		} else if (i < text.size() and text[i] == ' ') {
			++i;
		}
	}
	return plain;
}

struct MenuFixture : catalist::test::TempDirFixture {
	MenuFixture() : previous_dir(std::filesystem::current_path()) {
		std::filesystem::current_path(TempDir());
	}
	~MenuFixture() { std::filesystem::current_path(previous_dir); }

	int Run(const std::string& typed) {
		catalist::Catalogue db("catalist.csv");
		REQUIRE(db.Load().has_value());
		std::istringstream in_stream(typed);
		std::ostringstream out_stream;
		catalist::App app{db, in_stream, out_stream, TestClock, web.Link()};
		app.columns = columns;
		int exit_code = catalist::RunMenu(app);
		raw = out_stream.str();
		out = Plain(raw);
		return exit_code;
	}

	std::vector<catalist::Entry> Saved(MediaType type) {
		catalist::Catalogue db("catalist.csv");
		REQUIRE(db.Load().has_value());
		return db.GetEntries(type);
	}

	catalist::Entry SavedTitle(MediaType type, const std::string& title) {
		catalist::Catalogue db("catalist.csv");
		REQUIRE(db.Load().has_value());
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
		catalist::Catalogue db("catalist.csv");
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
		REQUIRE(db.Save("2026-10-19 10:00:00"));
	}

	catalist::test::FakeWeb web;
	std::filesystem::path previous_dir;
	// What the menu printed, then the same with every numeric or alpha circle
	// read as its digit or letter, the arrow as "->", and every other icon
	// dropped with the space after it.
	std::string raw;
	std::string out;
	int columns = 80;
};

inline bool Has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_MENU_FIXTURE_HPP_
