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

TitleList TitlesSortedBy(std::vector<catalist::Entry> entries, SortKey key) {
	catalist::SortEntries(entries, key);
	TitleList titles;
	for (const auto& e : entries)
		titles.push_back(catalist::DisplayName(e));
	return titles;
}

}  // namespace

TEST_CASE("sortEntries orders by the shown name") {
	std::vector<catalist::Entry> entries = {MakeEntry().Title("Zeta").EnglishTitle("Alpha"),
	                                        MakeEntry().Title("Bravo")};
	CHECK(TitlesSortedBy(entries, SortKey::kTitle) == TitleList{"Alpha", "Bravo"});
}

TEST_CASE("sortEntries ignores capitals and punctuation") {
	std::vector<catalist::Entry> entries = {
	    MakeEntry().Title("Zom 100"), MakeEntry().Title("citrus"), MakeEntry().Title("\"Alpha\"")};
	CHECK(TitlesSortedBy(entries, SortKey::kTitle) == TitleList{"\"Alpha\"", "citrus", "Zom 100"});
}

TEST_CASE("sortEntries puts the latest change first, then breaks ties by name") {
	catalist::Entry day = MakeEntry().Title("Day only").UpdatedAt("2026-11-24");
	catalist::Entry early = MakeEntry().Title("Early");
	early.changed_at = "2026-11-24 09:00:00";
	catalist::Entry late = MakeEntry().Title("Late");
	late.changed_at = "2026-11-24 10:00:00";
	catalist::Entry tied = MakeEntry().Title("Also late");
	tied.changed_at = late.changed_at;
	CHECK(TitlesSortedBy({day, early, late, tied}, SortKey::kUpdated) ==
	      TitleList{"Also late", "Late", "Early", "Day only"});
}

TEST_CASE("sortEntries orders by status, score, progress and start date") {
	std::vector<catalist::Entry> entries = {
	    MakeEntry().Title("Planned").Status(WatchStatus::kPlanToWatch),
	    MakeEntry()
	        .Title("Done")
	        .Status(WatchStatus::kCompleted)
	        .Score(9)
	        .Progress(12)
	        .StartDate("2024-01-01"),
	    MakeEntry().Title("Again").Status(WatchStatus::kRewatching).Score(6).Progress(3),
	    MakeEntry()
	        .Title("Now")
	        .Status(WatchStatus::kWatching)
	        .Score(9)
	        .Progress(5)
	        .StartDate("2026-01-01")};
	CHECK(TitlesSortedBy(entries, SortKey::kStatus) ==
	      TitleList{"Now", "Again", "Done", "Planned"});
	CHECK(TitlesSortedBy(entries, SortKey::kScore) == TitleList{"Done", "Now", "Again", "Planned"});
	CHECK(TitlesSortedBy(entries, SortKey::kProgress) ==
	      TitleList{"Done", "Now", "Again", "Planned"});
	CHECK(TitlesSortedBy(entries, SortKey::kStartDate) ==
	      TitleList{"Now", "Done", "Again", "Planned"});
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
