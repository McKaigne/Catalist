#include "storage/journal.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "support/test_support.hpp"

namespace journal = catalist::journal;
using catalist::Entry;
using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::MakeEntry;
using catalist::test::RequireDate;

namespace {

Entry Taxi() {
	return MakeEntry()
	    .Id(2)
	    .Title("Odd Taxi")
	    .MalId(44265)
	    .AnilistId(128547)
	    .Status(WatchStatus::kWatching)
	    .Score(8)
	    .Progress(5)
	    .TotalUnits(13)
	    .UpdatedAt("2026-10-01");
}

Entry Frieren() {
	return MakeEntry().Id(1).Title("Sousou no Frieren").MalId(52991).AnilistId(154587);
}

Entry WithProgress(Entry of, int progress) {
	of.progress = progress;
	of.updated_at = RequireDate("2026-10-20");
	return of;
}

}  // namespace

TEST_CASE("journal diff finds what was added, changed and removed, by id") {
	std::vector<Entry> saved = {Frieren(), Taxi()};
	Entry added = MakeEntry().Id(3).Title("Dandadan");
	std::vector<Entry> current = {WithProgress(Taxi(), 6), added};

	auto changes = journal::Diff(saved, current);
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

	CHECK(journal::Diff(saved, saved).empty());
	CHECK(journal::Diff({}, {}).empty());
}

TEST_CASE("journal diff leaves out what only the web fills in") {
	Entry checked = Taxi();
	checked.total_volumes = 4;
	checked.anilist_id = 9;
	checked.mal_id = 8;
	checked.updated_at = RequireDate("2026-10-20");
	CHECK(journal::Diff({Taxi()}, {checked}).empty());

	// Every field the person sets is one that counts.
	std::vector<Entry> each(9, Taxi());
	each[0].title = "ODDTAXI";
	each[1].status = WatchStatus::kOnHold;
	each[2].score = 9;
	each[3].progress = 6;
	each[4].total_units = 14;
	each[5].volumes = 1;
	each[6].rewatched = 1;
	each[7].start_date = RequireDate("2026-10-01");
	each[8].finish_date = RequireDate("2026-10-02");
	for (const auto& changed : each)
		CHECK(journal::Diff({Taxi()}, {changed}).size() == 1);
}

TEST_CASE("journal changedFields names each field with its old and new value") {
	Entry after = Taxi();
	after.progress = 13;
	after.status = WatchStatus::kCompleted;
	after.score = 9;
	after.finish_date = RequireDate("2026-10-20");
	after.title = "ODDTAXI";
	auto fields = journal::ChangedFields({MediaType::kAnime, 2, Taxi(), after});
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

	auto added = journal::ChangedFields({MediaType::kAnime, 2, std::nullopt, Taxi()});
	REQUIRE(added.size() == 1);
	CHECK(added[0].field == "added");
	CHECK(added[0].before == "-");
	CHECK(added[0].after == "watching");
	auto removed = journal::ChangedFields({MediaType::kAnime, 2, Taxi(), std::nullopt});
	REQUIRE(removed.size() == 1);
	CHECK(removed[0].field == "removed");
	CHECK(removed[0].before == "watching");
	CHECK(removed[0].after == "-");

	CHECK(journal::ChangedFields({MediaType::kAnime, 2, Taxi(), Taxi()}).empty());
	CHECK(journal::ChangedFields({}).empty());
}

TEST_CASE("journal transition sets back only the fields the change set") {
	Entry before = Taxi();
	Entry after = WithProgress(Taxi(), 6);
	// Since then the web found the volumes and the person rated the title.
	Entry now = after;
	now.total_volumes = 13;
	now.score = 9;

	auto undone = journal::Transition({Frieren(), now}, after, before, false);
	REQUIRE(undone.has_value());
	REQUIRE(undone.value());
	CHECK(undone.value()->progress == 5);
	CHECK(undone.value()->score == 9);
	CHECK(undone.value()->total_volumes == 13);
	CHECK(undone.value()->updated_at == RequireDate("2026-10-01"));

	// Redo is the same step the other way.
	auto redone = journal::Transition({*undone.value()}, before, after, false);
	REQUIRE(redone.has_value());
	CHECK(redone.value()->progress == 6);
	CHECK(redone.value()->updated_at == RequireDate("2026-10-20"));
}

TEST_CASE("journal transition refuses when the entry has changed since, unless forced") {
	Entry before = Taxi();
	Entry after = WithProgress(Taxi(), 6);
	Entry now = WithProgress(Taxi(), 7);

	auto refused = journal::Transition({now}, after, before, false);
	REQUIRE_FALSE(refused.has_value());
	CHECK(refused.error() == "Odd Taxi (a:2) has changed since: progress is now 7, not 6");

	auto forced = journal::Transition({now}, after, before, true);
	REQUIRE(forced.has_value());
	CHECK(forced.value()->progress == 5);

	// An added Entry is taken away only as it was added.
	refused = journal::Transition({now}, after, std::nullopt, false);
	REQUIRE_FALSE(refused.has_value());
	CHECK(refused.error() == "Odd Taxi (a:2) has changed since: progress is now 7, not 6");
	auto taken = journal::Transition({after}, after, std::nullopt, false);
	REQUIRE(taken.has_value());
	CHECK_FALSE(taken.value());
	forced = journal::Transition({now}, after, std::nullopt, true);
	REQUIRE(forced.has_value());
	CHECK_FALSE(forced.value());
}

TEST_CASE("journal transition says when the entry is gone") {
	Entry before = Taxi();
	Entry after = WithProgress(Taxi(), 6);
	for (bool force : {false, true}) {
		auto gone = journal::Transition({Frieren()}, after, before, force);
		REQUIRE_FALSE(gone.has_value());
		CHECK(gone.error() == "Odd Taxi (a:2) is no longer in your lists");
	}
	// Taking away what is gone already: only when forced.
	CHECK_FALSE(journal::Transition({}, after, std::nullopt, false).has_value());
	auto forced = journal::Transition({}, after, std::nullopt, true);
	REQUIRE(forced.has_value());
	CHECK_FALSE(forced.value());
}

TEST_CASE("journal transition brings a removed entry back under its id") {
	auto back = journal::Transition({Frieren()}, std::nullopt, Taxi(), false);
	REQUIRE(back.has_value());
	REQUIRE(back.value());
	CHECK(back.value()->id == 2);
	CHECK(back.value()->title == "Odd Taxi");
	CHECK(back.value()->anilist_id == 128547);

	// Its ref went to another title.
	Entry other = MakeEntry().Id(2).Title("Mushishi");
	for (bool force : {false, true}) {
		auto taken = journal::Transition({Frieren(), other}, std::nullopt, Taxi(), force);
		REQUIRE_FALSE(taken.has_value());
		CHECK(taken.error() == "Odd Taxi (a:2) cannot come back: its ref is now that of Mushishi");
	}
	// The title was added again.
	Entry again = MakeEntry().Id(3).Title("ODDTAXI").AnilistId(128547);
	auto twice = journal::Transition({Frieren(), again}, std::nullopt, Taxi(), false);
	REQUIRE_FALSE(twice.has_value());
	CHECK(twice.error() == "Odd Taxi is in your lists again, as ODDTAXI (a:3)");
	Entry by_mal = MakeEntry().Id(3).Title("ODDTAXI").MalId(44265);
	CHECK_FALSE(journal::Transition({by_mal}, std::nullopt, Taxi(), true).has_value());
	// Titles without site ids are never the same title.
	Entry by_hand = MakeEntry().Id(7).Title("By hand");
	Entry also_by_hand = MakeEntry().Id(8).Title("By hand");
	CHECK(journal::Transition({also_by_hand}, std::nullopt, by_hand, false).has_value());

	auto nothing = journal::Transition({}, std::nullopt, std::nullopt, false);
	REQUIRE(nothing.has_value());
	CHECK_FALSE(nothing.value());
}
