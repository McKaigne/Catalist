#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/lists.hpp"
#include "support/sampleLibrary.hpp"
#include "support/testSupport.hpp"

using catalist::mediaType;
using catalist::sortKey;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::sampleList;

namespace {

using titleList = std::vector<std::string>;

titleList titlesSortedBy(std::vector<catalist::entry> entries, sortKey key, bool reverse) {
	catalist::sortEntries(entries, key, reverse);
	titleList titles;
	for (const auto& e : entries)
		titles.push_back(e.title);
	return titles;
}

}  // namespace

TEST_CASE("sortEntries puts each key in its natural order and reverse flips it") {
	auto anime = sampleList(mediaType::kAnime);
	const titleList byRating = {"Sousou no Frieren", "Odd Taxi", "Ghost, \"in\" the Shell",
	                            "A Very Long Title That Will Definitely Be "
	                            "Truncated"};
	CHECK(titlesSortedBy(anime, sortKey::kScore, false) == byRating);
	CHECK(titlesSortedBy(anime, sortKey::kScore, true) ==
	      titleList(byRating.rbegin(), byRating.rend()));
	CHECK(titlesSortedBy(anime, sortKey::kProgress, false) == byRating);

	CHECK(titlesSortedBy(anime, sortKey::kTitle, false) ==
	      titleList{"A Very Long Title That Will Definitely Be Truncated",
	                "Ghost, \"in\" the Shell", "Odd Taxi", "Sousou no Frieren"});
	CHECK(titlesSortedBy(anime, sortKey::kStatus, false) ==
	      titleList{"Odd Taxi", "Sousou no Frieren", "Ghost, \"in\" the Shell",
	                "A Very Long Title That Will Definitely Be Truncated"});
	// By the date the list shows: finish when completed, else start, else the
	// last change.
	CHECK(titlesSortedBy(anime, sortKey::kDate, false) ==
	      titleList{"A Very Long Title That Will Definitely Be Truncated", "Sousou no Frieren",
	                "Odd Taxi", "Ghost, \"in\" the Shell"});
	// Most new episodes first: Odd Taxi has 8 out that are not seen.
	CHECK(titlesSortedBy(anime, sortKey::kNew, false).front() == "Odd Taxi");
}

TEST_CASE("sortEntries breaks ties by title") {
	std::vector<catalist::entry> scored = {makeEntry().title("Charlie").score(7),
	                                       makeEntry().title("Alpha").score(7),
	                                       makeEntry().title("Bravo").score(9)};
	CHECK(titlesSortedBy(scored, sortKey::kScore, false) == titleList{"Bravo", "Alpha", "Charlie"});
}

TEST_CASE("list statistics count, average and estimate time") {
	auto anime = sampleList(mediaType::kAnime);
	auto manga = sampleList(mediaType::kManga);
	CHECK(catalist::countByStatus(anime, watchStatus::kWatching) == 1);
	CHECK(catalist::countByStatus(anime, watchStatus::kOnHold) == 0);
	CHECK(catalist::countByStatus(manga, watchStatus::kOnHold) == 1);
	// Unrated titles do not pull the mean down.
	CHECK(catalist::averageScore(anime) == doctest::Approx(8.0));
	CHECK(catalist::averageScore(manga) == doctest::Approx(9.5));
	CHECK(catalist::totalProgress(anime) == 36);
	CHECK(catalist::totalProgress(manga) == 461);
	CHECK(catalist::totalVolumes(manga) == 53);
	CHECK(catalist::totalRewatched(anime) == 1);
	CHECK(catalist::daysSpent(anime) == doctest::Approx(36 * 24 / 1440.0));
	CHECK(catalist::daysSpent(manga) == doctest::Approx(461 * 5 / 1440.0));

	CHECK(catalist::averageScore({}) == 0.0);
	CHECK(catalist::daysSpent({}) == 0.0);
}
