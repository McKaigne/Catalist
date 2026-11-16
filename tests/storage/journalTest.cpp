#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "storage/journal.hpp"
#include "support/testSupport.hpp"

namespace journal = catalist::journal;
using catalist::entry;
using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::requireDate;

namespace {

entry taxi() {
	return makeEntry()
	    .id(2)
	    .title("Odd Taxi")
	    .malId(44265)
	    .anilistId(128547)
	    .status(watchStatus::kWatching)
	    .score(8)
	    .progress(5)
	    .totalUnits(13)
	    .updatedAt("2026-10-01");
}

entry frieren() {
	return makeEntry().id(1).title("Sousou no Frieren").malId(52991).anilistId(154587);
}

entry withProgress(entry of, int progress) {
	of.progress = progress;
	of.updatedAt = requireDate("2026-10-20");
	return of;
}

}  // namespace

TEST_CASE("journal diff finds what was added, changed and removed, by id") {
	std::vector<entry> saved = {frieren(), taxi()};
	entry added = makeEntry().id(3).title("Dandadan");
	std::vector<entry> current = {withProgress(taxi(), 6), added};

	auto changes = journal::diff(saved, current);
	REQUIRE(changes.size() == 3);
	CHECK(changes[0].id == 2);
	CHECK(changes[0].before->progress == 5);
	CHECK(changes[0].after->progress == 6);
	CHECK(changes[1].id == 3);
	CHECK_FALSE(changes[1].before);
	CHECK(changes[1].after->title == "Dandadan");
	CHECK(changes[2].id == 1);
	CHECK(changes[2].before->title == "Sousou no Frieren");
	CHECK_FALSE(changes[2].after);

	CHECK(journal::diff(saved, saved).empty());
	CHECK(journal::diff({}, {}).empty());
}

TEST_CASE("journal diff leaves out what only the web fills in") {
	entry checked = taxi();
	checked.released = 13;
	checked.totalVolumes = 4;
	checked.anilistId = 9;
	checked.malId = 8;
	checked.checkedAt = requireDate("2026-10-20");
	checked.updatedAt = requireDate("2026-10-20");
	CHECK(journal::diff({taxi()}, {checked}).empty());

	// Every field the person sets is one that counts.
	std::vector<entry> each(9, taxi());
	each[0].title = "ODDTAXI";
	each[1].status = watchStatus::kOnHold;
	each[2].score = 9;
	each[3].progress = 6;
	each[4].totalUnits = 14;
	each[5].volumes = 1;
	each[6].rewatched = 1;
	each[7].startDate = requireDate("2026-10-01");
	each[8].finishDate = requireDate("2026-10-02");
	for (const auto& changed : each)
		CHECK(journal::diff({taxi()}, {changed}).size() == 1);
}

TEST_CASE("journal changedFields names each field with its old and new value") {
	entry after = taxi();
	after.progress = 13;
	after.status = watchStatus::kCompleted;
	after.score = 9;
	after.finishDate = requireDate("2026-10-20");
	after.title = "ODDTAXI";
	auto fields = journal::changedFields({mediaType::kAnime, 2, taxi(), after});
	REQUIRE(fields.size() == 5);
	CHECK(fields[0].field == "progress");
	CHECK(fields[0].before == "5");
	CHECK(fields[0].after == "13");
	CHECK(fields[1].field == "status");
	CHECK(fields[1].before == "watching");
	CHECK(fields[1].after == "completed");
	CHECK(fields[2].field == "score");
	CHECK(fields[3].field == "finished");
	CHECK(fields[3].before == "-");
	CHECK(fields[3].after == "2026-10-20");
	CHECK(fields[4].field == "title");
	CHECK(fields[4].after == "ODDTAXI");

	auto added = journal::changedFields({mediaType::kAnime, 2, std::nullopt, taxi()});
	REQUIRE(added.size() == 1);
	CHECK(added[0].field == "added");
	CHECK(added[0].before == "-");
	CHECK(added[0].after == "watching");
	auto removed = journal::changedFields({mediaType::kAnime, 2, taxi(), std::nullopt});
	REQUIRE(removed.size() == 1);
	CHECK(removed[0].field == "removed");
	CHECK(removed[0].before == "watching");
	CHECK(removed[0].after == "-");

	CHECK(journal::changedFields({mediaType::kAnime, 2, taxi(), taxi()}).empty());
	CHECK(journal::changedFields({}).empty());
}

TEST_CASE("journal transition sets back only the fields the change set") {
	entry before = taxi();
	entry after = withProgress(taxi(), 6);
	// Since then the web found the releases and the person rated the title.
	entry now = after;
	now.released = 13;
	now.score = 9;

	auto undone = journal::transition({frieren(), now}, after, before, false);
	REQUIRE(undone.has_value());
	REQUIRE(undone.value());
	CHECK(undone.value()->progress == 5);
	CHECK(undone.value()->score == 9);
	CHECK(undone.value()->released == 13);
	CHECK(undone.value()->updatedAt == requireDate("2026-10-01"));

	// Redo is the same step the other way.
	auto redone = journal::transition({*undone.value()}, before, after, false);
	REQUIRE(redone.has_value());
	CHECK(redone.value()->progress == 6);
	CHECK(redone.value()->updatedAt == requireDate("2026-10-20"));
}

TEST_CASE("journal transition refuses when the entry has changed since, unless forced") {
	entry before = taxi();
	entry after = withProgress(taxi(), 6);
	entry now = withProgress(taxi(), 7);

	auto refused = journal::transition({now}, after, before, false);
	REQUIRE_FALSE(refused.has_value());
	CHECK(refused.error() == "Odd Taxi (a:2) has changed since: progress is now 7, not 6");

	auto forced = journal::transition({now}, after, before, true);
	REQUIRE(forced.has_value());
	CHECK(forced.value()->progress == 5);

	// An added entry is taken away only as it was added.
	refused = journal::transition({now}, after, std::nullopt, false);
	REQUIRE_FALSE(refused.has_value());
	CHECK(refused.error() == "Odd Taxi (a:2) has changed since: progress is now 7, not 6");
	auto taken = journal::transition({after}, after, std::nullopt, false);
	REQUIRE(taken.has_value());
	CHECK_FALSE(taken.value());
	forced = journal::transition({now}, after, std::nullopt, true);
	REQUIRE(forced.has_value());
	CHECK_FALSE(forced.value());
}

TEST_CASE("journal transition says when the entry is gone") {
	entry before = taxi();
	entry after = withProgress(taxi(), 6);
	for (bool force : {false, true}) {
		auto gone = journal::transition({frieren()}, after, before, force);
		REQUIRE_FALSE(gone.has_value());
		CHECK(gone.error() == "Odd Taxi (a:2) is no longer in your lists");
	}
	// Taking away what is gone already: only when forced.
	CHECK_FALSE(journal::transition({}, after, std::nullopt, false).has_value());
	auto forced = journal::transition({}, after, std::nullopt, true);
	REQUIRE(forced.has_value());
	CHECK_FALSE(forced.value());
}

TEST_CASE("journal transition brings a removed entry back under its id") {
	auto back = journal::transition({frieren()}, std::nullopt, taxi(), false);
	REQUIRE(back.has_value());
	REQUIRE(back.value());
	CHECK(back.value()->id == 2);
	CHECK(back.value()->title == "Odd Taxi");
	CHECK(back.value()->anilistId == 128547);

	// Its ref went to another title.
	entry other = makeEntry().id(2).title("Mushishi");
	for (bool force : {false, true}) {
		auto taken = journal::transition({frieren(), other}, std::nullopt, taxi(), force);
		REQUIRE_FALSE(taken.has_value());
		CHECK(taken.error() == "Odd Taxi (a:2) cannot come back: its ref is now that of Mushishi");
	}
	// The title was added again.
	entry again = makeEntry().id(3).title("ODDTAXI").anilistId(128547);
	auto twice = journal::transition({frieren(), again}, std::nullopt, taxi(), false);
	REQUIRE_FALSE(twice.has_value());
	CHECK(twice.error() == "Odd Taxi is in your lists again, as ODDTAXI (a:3)");
	entry byMal = makeEntry().id(3).title("ODDTAXI").malId(44265);
	CHECK_FALSE(journal::transition({byMal}, std::nullopt, taxi(), true).has_value());
	// Titles without site ids are never the same title.
	entry byHand = makeEntry().id(7).title("By hand");
	entry alsoByHand = makeEntry().id(8).title("By hand");
	CHECK(journal::transition({alsoByHand}, std::nullopt, byHand, false).has_value());

	auto nothing = journal::transition({}, std::nullopt, std::nullopt, false);
	REQUIRE(nothing.has_value());
	CHECK_FALSE(nothing.value());
}
