#include "core/lists.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "support/sample_library.hpp"
#include "support/test_support.hpp"

using catalist::MediaType;
using catalist::SortKey;
using catalist::WatchStatus;
using catalist::test::MakeEntry;
using catalist::test::SampleList;

namespace {

using TitleList = std::vector<std::string>;

TitleList TitlesSortedBy(std::vector<catalist::Entry> entries, SortKey key, bool reverse) {
	catalist::SortEntries(entries, key, reverse);
	TitleList titles;
	for (const auto& e : entries)
		titles.push_back(e.title);
	return titles;
}

}  // namespace

TEST_CASE("sortEntries puts each key in its natural order and reverse flips it") {
	auto anime = SampleList(MediaType::kAnime);
	const TitleList by_rating = {"Sousou no Frieren", "Odd Taxi", "Ghost, \"in\" the Shell",
	                             "A Very Long Title That Will Definitely Be "
	                             "Truncated"};
	CHECK(TitlesSortedBy(anime, SortKey::kScore, false) == by_rating);
	CHECK(TitlesSortedBy(anime, SortKey::kScore, true) ==
	      TitleList(by_rating.rbegin(), by_rating.rend()));
	CHECK(TitlesSortedBy(anime, SortKey::kProgress, false) == by_rating);

	CHECK(TitlesSortedBy(anime, SortKey::kTitle, false) ==
	      TitleList{"A Very Long Title That Will Definitely Be Truncated",
	                "Ghost, \"in\" the Shell", "Odd Taxi", "Sousou no Frieren"});
	CHECK(TitlesSortedBy(anime, SortKey::kStatus, false) ==
	      TitleList{"Odd Taxi", "Sousou no Frieren", "Ghost, \"in\" the Shell",
	                "A Very Long Title That Will Definitely Be Truncated"});
	CHECK(TitlesSortedBy(anime, SortKey::kDate, false) ==
	      TitleList{"A Very Long Title That Will Definitely Be Truncated", "Sousou no Frieren",
	                "Odd Taxi", "Ghost, \"in\" the Shell"});
}

TEST_CASE("sortEntries breaks ties by title") {
	std::vector<catalist::Entry> scored = {MakeEntry().Title("Charlie").Score(7),
	                                       MakeEntry().Title("Alpha").Score(7),
	                                       MakeEntry().Title("Bravo").Score(9)};
	CHECK(TitlesSortedBy(scored, SortKey::kScore, false) == TitleList{"Bravo", "Alpha", "Charlie"});
}

TEST_CASE("list statistics count, average and estimate time") {
	auto anime = SampleList(MediaType::kAnime);
	auto manga = SampleList(MediaType::kManga);
	CHECK(catalist::CountByStatus(anime, WatchStatus::kWatching) == 1);
	CHECK(catalist::CountByStatus(anime, WatchStatus::kOnHold) == 0);
	CHECK(catalist::CountByStatus(manga, WatchStatus::kOnHold) == 1);
	CHECK(catalist::AverageScore(anime) == doctest::Approx(8.0));
	CHECK(catalist::AverageScore(manga) == doctest::Approx(9.5));
	CHECK(catalist::TotalProgress(anime) == 36);
	CHECK(catalist::TotalProgress(manga) == 461);
	CHECK(catalist::TotalVolumes(manga) == 53);
	CHECK(catalist::TotalRewatched(anime) == 1);
	CHECK(catalist::DaysSpent(anime) == doctest::Approx(36 * 24 / 1440.0));
	CHECK(catalist::DaysSpent(manga) == doctest::Approx(461 * 5 / 1440.0));

	CHECK(catalist::AverageScore({}) == 0.0);
	CHECK(catalist::DaysSpent({}) == 0.0);
}
