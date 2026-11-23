#include "core/entry.hpp"

#include <doctest/doctest.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "support/test_support.hpp"

using catalist::ParseStatus;
using catalist::WatchStatus;

namespace {

void CheckAllMapTo(const std::vector<std::string>& spellings, WatchStatus expected) {
	for (const auto& spelling : spellings) {
		CAPTURE(spelling);
		CHECK(ParseStatus(spelling) == expected);
	}
}

}  // namespace

TEST_CASE("parseStatus watching spellings") {
	CheckAllMapTo({"watching", "Watching", "WATCHING", "watch", "reading", "Reading", "read", "1"},
	              WatchStatus::kWatching);
}

TEST_CASE("parseStatus rewatching spellings") {
	CheckAllMapTo({"rewatching", "Rewatching", "rereading", "Rereading", "REPEATING"},
	              WatchStatus::kRewatching);
}

TEST_CASE("parseStatus completed spellings") {
	CheckAllMapTo({"completed", "Completed", "complete", "2"}, WatchStatus::kCompleted);
}

TEST_CASE("parseStatus on hold spellings") {
	CheckAllMapTo({"on-hold", "On-Hold", "onhold", "on hold", "hold", "3"}, WatchStatus::kOnHold);
}

TEST_CASE("parseStatus dropped spellings") {
	CheckAllMapTo({"dropped", "Dropped", "drop", "4"}, WatchStatus::kDropped);
}

TEST_CASE("parseStatus plan spellings") {
	CheckAllMapTo({"plan to watch", "Plan to Watch", "Plan to Read", "planned", "plan",
	               "plan-to-watch", "plan-to-read", "5"},
	              WatchStatus::kPlanToWatch);
}

TEST_CASE("parseStatus rejects unknown input") {
	for (const char* spelling : {"", "xyz", "0", "6", "ptw"}) {
		CAPTURE(spelling);
		CHECK_FALSE(ParseStatus(spelling).has_value());
	}
}

TEST_CASE("statusToString round trips through parseStatus") {
	for (auto type : {catalist::MediaType::kAnime, catalist::MediaType::kManga}) {
		for (auto status : {WatchStatus::kWatching, WatchStatus::kCompleted, WatchStatus::kOnHold,
		                    WatchStatus::kDropped, WatchStatus::kPlanToWatch}) {
			CHECK(ParseStatus(catalist::StatusToString(status, type)) == status);
		}
	}
}

using catalist::CountField;
using catalist::Entry;
using catalist::FormatDate;
using catalist::IsoDate;
using catalist::WatchStatus;
using catalist::test::MakeEntry;
using catalist::test::RequireDate;

namespace {

const IsoDate kToday = RequireDate("2026-10-20");

Entry Watching(int progress, int total) {
	return MakeEntry()
	    .Id(1)
	    .Title("Show")
	    .Status(WatchStatus::kWatching)
	    .Progress(progress)
	    .TotalUnits(total)
	    .StartDate("2026-01-01");
}

}  // namespace

TEST_CASE("stepProgress clamps at zero and at the total") {
	CHECK(catalist::StepProgress(Watching(5, 12), -9, kToday).progress == 0);

	auto finished = catalist::StepProgress(Watching(5, 12), 40, kToday);
	CHECK(finished.progress == 12);
	CHECK(finished.status == WatchStatus::kCompleted);
	CHECK(FormatDate(finished.finish_date) == "2026-10-20");
	CHECK(FormatDate(finished.updated_at) == "2026-10-20");
}

TEST_CASE("stepProgress with an unknown total never completes") {
	auto open = catalist::StepProgress(Watching(5, 0), 495, kToday);
	CHECK(open.progress == 500);
	CHECK(open.status == WatchStatus::kWatching);
	CHECK_FALSE(open.finish_date.has_value());
}

TEST_CASE("stepProgress leaves a finished or empty entry exactly as it was") {
	// Pressing + on a completed title used to stamp today as its start and
	// finish date.
	Entry done = MakeEntry()
	                 .Status(WatchStatus::kCompleted)
	                 .Progress(28)
	                 .TotalUnits(28)
	                 .UpdatedAt("2024-01-01");
	auto same = catalist::StepProgress(done, 1, kToday);
	CHECK_FALSE(same.start_date.has_value());
	CHECK_FALSE(same.finish_date.has_value());
	CHECK(FormatDate(same.updated_at) == "2024-01-01");

	auto at_zero = catalist::StepProgress(Watching(0, 12), -1, kToday);
	CHECK_FALSE(at_zero.updated_at.has_value());
}

TEST_CASE("stepProgress forward starts a planned entry") {
	Entry planned = MakeEntry().Status(WatchStatus::kPlanToWatch).TotalUnits(3);
	auto started = catalist::StepProgress(planned, 1, kToday);
	CHECK(started.progress == 1);
	CHECK(started.status == WatchStatus::kWatching);
	CHECK(FormatDate(started.start_date) == "2026-10-20");
	CHECK_FALSE(started.finish_date.has_value());
}

TEST_CASE("stepProgress forward to the total completes with a finish date") {
	auto finished = catalist::StepProgress(Watching(11, 12), 1, kToday);
	CHECK(finished.status == WatchStatus::kCompleted);
	CHECK(FormatDate(finished.finish_date) == "2026-10-20");
	CHECK(FormatDate(finished.start_date) == "2026-01-01");
}

TEST_CASE("stepProgress back below the total reopens a completed entry") {
	Entry done = MakeEntry()
	                 .Status(WatchStatus::kCompleted)
	                 .Progress(12)
	                 .TotalUnits(12)
	                 .FinishDate("2026-05-05");
	auto reopened = catalist::StepProgress(done, -1, kToday);
	CHECK(reopened.progress == 11);
	CHECK(reopened.status == WatchStatus::kWatching);
	CHECK_FALSE(reopened.finish_date.has_value());
}

TEST_CASE("stepProgress back stops at zero") {
	CHECK(catalist::StepProgress(Watching(0, 12), -1, kToday).progress == 0);
}

TEST_CASE("setStatus to completed fills progress and finish date") {
	auto done = catalist::SetStatus(Watching(4, 12), WatchStatus::kCompleted, kToday);
	CHECK(done.status == WatchStatus::kCompleted);
	CHECK(done.progress == 12);
	CHECK(FormatDate(done.finish_date) == "2026-10-20");
}

TEST_CASE("setStatus from completed back to watching clears the finish date") {
	Entry done = MakeEntry()
	                 .Status(WatchStatus::kCompleted)
	                 .FinishDate("2026-05-05")
	                 .StartDate("2026-01-01");
	auto resumed = catalist::SetStatus(done, WatchStatus::kWatching, kToday);
	CHECK(resumed.status == WatchStatus::kWatching);
	CHECK_FALSE(resumed.finish_date.has_value());
}

TEST_CASE("only a completed or rewatching title keeps a finish date") {
	Entry done = MakeEntry().Status(WatchStatus::kCompleted).FinishDate("2026-05-05");
	for (WatchStatus status :
	     {WatchStatus::kOnHold, WatchStatus::kDropped, WatchStatus::kPlanToWatch}) {
		auto stopped = catalist::SetStatus(done, status, kToday);
		CHECK(stopped.status == status);
		CHECK_FALSE(stopped.finish_date.has_value());
	}
	auto dropped = catalist::SetStatus(MakeEntry(), WatchStatus::kDropped, kToday);
	CHECK_FALSE(dropped.finish_date.has_value());
	Entry stale = MakeEntry().Status(WatchStatus::kOnHold).FinishDate("2026-05-05");
	CHECK_FALSE(catalist::SetStatus(stale, WatchStatus::kWatching, kToday).finish_date);
	CHECK_FALSE(catalist::StepProgress(stale, 1, kToday).finish_date);
}

TEST_CASE("stopping a rewatch returns the title to completed") {
	Entry again = MakeEntry()
	                  .Status(WatchStatus::kRewatching)
	                  .TotalUnits(12)
	                  .Progress(4)
	                  .Rewatched(1)
	                  .FinishDate("2026-02-01");
	for (WatchStatus status :
	     {WatchStatus::kOnHold, WatchStatus::kDropped, WatchStatus::kPlanToWatch}) {
		auto back = catalist::SetStatus(again, status, kToday);
		CHECK(back.status == WatchStatus::kCompleted);
		CHECK(back.progress == 12);
		CHECK(back.rewatched == 1);
		CHECK(FormatDate(back.finish_date) == "2026-02-01");
	}
	Entry unknown = again;
	unknown.total_units = 0;
	CHECK(catalist::SetStatus(unknown, WatchStatus::kDropped, kToday).progress == 4);
	CHECK(catalist::SetStatus(again, WatchStatus::kWatching, kToday) == again);
	auto finished = catalist::SetStatus(again, WatchStatus::kCompleted, kToday);
	CHECK(finished.rewatched == 2);
	CHECK(FormatDate(finished.finish_date) == "2026-02-01");
}

TEST_CASE("a status a title cannot take is refused") {
	Entry done = MakeEntry().Title("Done").Status(WatchStatus::kCompleted);
	CHECK(catalist::StatusRefusal(done, WatchStatus::kWatching).has_value());
	CHECK_FALSE(catalist::StatusRefusal(done, WatchStatus::kRewatching).has_value());
	Entry open = MakeEntry().Title("Open").Status(WatchStatus::kOnHold);
	CHECK(catalist::StatusRefusal(open, WatchStatus::kRewatching).has_value());
	CHECK_FALSE(catalist::StatusRefusal(open, WatchStatus::kWatching).has_value());
}

TEST_CASE("going back reopens a completed title of unknown length") {
	Entry done = MakeEntry().Status(WatchStatus::kCompleted).Progress(5).FinishDate("2026-02-01");
	auto back = catalist::StepProgress(done, -1, kToday);
	CHECK(back.status == WatchStatus::kWatching);
	CHECK_FALSE(back.finish_date.has_value());
}

TEST_CASE("setScore stays within zero and ten") {
	CHECK(catalist::SetScore(MakeEntry().Score(9), 12, kToday).score == 10);
	CHECK(catalist::SetScore(MakeEntry().Score(1), -2, kToday).score == 0);
	CHECK(FormatDate(catalist::SetScore(MakeEntry(), 1, kToday).updated_at) == "2026-10-20");
	// The score it has already: nothing changes, not even the date.
	CHECK_FALSE(catalist::SetScore(MakeEntry().Score(10), 11, kToday).updated_at.has_value());
}

TEST_CASE("setCount types progress in like a long step") {
	Entry planned = MakeEntry().Status(WatchStatus::kPlanToWatch).TotalUnits(24);
	auto started = catalist::SetCount(planned, CountField::kProgress, 10, kToday);
	CHECK(started.progress == 10);
	CHECK(started.status == WatchStatus::kWatching);
	CHECK(FormatDate(started.start_date) == "2026-10-20");

	auto done = catalist::SetCount(started, CountField::kProgress, 99, kToday);
	CHECK(done.progress == 24);
	CHECK(done.status == WatchStatus::kCompleted);

	auto reopened = catalist::SetCount(done, CountField::kProgress, 3, kToday);
	CHECK(reopened.progress == 3);
	CHECK(reopened.status == WatchStatus::kWatching);
	CHECK_FALSE(reopened.finish_date.has_value());
}

TEST_CASE("setCount sets totals, volumes and rewatches within bounds") {
	Entry show = Watching(10, 0);
	auto with_total = catalist::SetCount(show, CountField::kTotalUnits, 24, kToday);
	CHECK(with_total.total_units == 24);
	CHECK(with_total.progress == 10);
	CHECK(FormatDate(with_total.updated_at) == "2026-10-20");
	// A total below the progress lowers the progress; 0 means unknown.
	CHECK(catalist::SetCount(show, CountField::kTotalUnits, 6, kToday).progress == 6);
	CHECK(catalist::SetCount(with_total, CountField::kTotalUnits, 0, kToday).progress == 10);
	CHECK(catalist::SetCount(show, CountField::kTotalUnits, -5, kToday).total_units == 0);

	Entry book = MakeEntry().Type(catalist::MediaType::kManga).TotalVolumes(11);
	CHECK(catalist::SetCount(book, CountField::kVolumes, 4, kToday).volumes == 4);
	CHECK(catalist::SetCount(book, CountField::kVolumes, 40, kToday).volumes == 11);
	Entry open = MakeEntry().Type(catalist::MediaType::kManga);
	CHECK(catalist::SetCount(open, CountField::kVolumes, 40, kToday).volumes == 40);

	CHECK(catalist::SetCount(show, CountField::kRewatched, 2, kToday).rewatched == 2);
	CHECK(catalist::SetCount(show, CountField::kRewatched, -1, kToday).rewatched == 0);
	CHECK_FALSE(catalist::SetCount(show, CountField::kRewatched, 0, kToday).updated_at.has_value());
}

TEST_CASE("countOf reads each count") {
	Entry e = MakeEntry().Progress(1).TotalUnits(2).Volumes(3).Rewatched(4);
	CHECK(catalist::CountOf(e, CountField::kProgress) == 1);
	CHECK(catalist::CountOf(e, CountField::kTotalUnits) == 2);
	CHECK(catalist::CountOf(e, CountField::kVolumes) == 3);
	CHECK(catalist::CountOf(e, CountField::kRewatched) == 4);
}

TEST_CASE("mergeImported refreshes totals and fills only what the user left empty") {
	Entry mine = MakeEntry()
	                 .MalId(5)
	                 .Status(WatchStatus::kCompleted)
	                 .Score(8)
	                 .Progress(3)
	                 .TotalUnits(0)
	                 .StartDate("2026-01-01");
	Entry imported = MakeEntry()
	                     .MalId(5)
	                     .Score(2)
	                     .Progress(9)
	                     .TotalUnits(12)
	                     .TotalVolumes(3)
	                     .StartDate("2020-01-01")
	                     .FinishDate("2020-02-02");
	auto merged = catalist::MergeImported(mine, imported);
	CHECK(merged.score == 8);
	CHECK(merged.progress == 3);
	CHECK(merged.total_units == 12);
	CHECK(merged.total_volumes == 3);
	CHECK(FormatDate(merged.start_date) == "2026-01-01");
	CHECK(FormatDate(merged.finish_date) == "2020-02-02");
	mine.status = WatchStatus::kWatching;
	CHECK_FALSE(catalist::MergeImported(mine, imported).finish_date.has_value());
}

TEST_CASE("takeImported puts the file's values in place of the user's") {
	Entry mine = MakeEntry()
	                 .MalId(5)
	                 .Status(WatchStatus::kCompleted)
	                 .Score(8)
	                 .Progress(12)
	                 .TotalUnits(12)
	                 .StartDate("2026-01-01")
	                 .FinishDate("2026-02-02");
	Entry file = MakeEntry().MalId(5).Status(WatchStatus::kWatching).Progress(3);
	auto taken = catalist::TakeImported(mine, file);
	CHECK(taken.status == WatchStatus::kWatching);
	CHECK(taken.progress == 3);
	// What the file does not hold stays, but a title in progress has no finish date.
	CHECK(taken.score == 8);
	CHECK(FormatDate(taken.start_date) == "2026-01-01");
	CHECK_FALSE(taken.finish_date.has_value());
}

TEST_CASE("setStatus fills the dates like the cycle and ignores no change") {
	Entry planned = MakeEntry().Status(WatchStatus::kPlanToWatch).TotalUnits(12).Progress(3);
	auto done = catalist::SetStatus(planned, WatchStatus::kCompleted, kToday);
	CHECK(done.status == WatchStatus::kCompleted);
	CHECK(done.progress == 12);
	CHECK(FormatDate(done.start_date) == "2026-10-20");
	CHECK(FormatDate(done.finish_date) == "2026-10-20");
	auto same = catalist::SetStatus(planned, WatchStatus::kPlanToWatch, kToday);
	CHECK_FALSE(same.updated_at.has_value());
	CHECK(catalist::SetScore(planned, 14, kToday).score == 10);
	CHECK(catalist::SetScore(planned, 7, kToday).score == 7);
}

TEST_CASE("a rewatch starts over, keeps the first dates, and counts once it is done") {
	Entry done = MakeEntry()
	                 .Status(WatchStatus::kCompleted)
	                 .TotalUnits(3)
	                 .Progress(3)
	                 .StartDate("2026-01-01")
	                 .FinishDate("2026-02-01");
	auto again = catalist::SetStatus(done, WatchStatus::kRewatching, kToday);
	CHECK(again.status == WatchStatus::kRewatching);
	CHECK(again.progress == 0);
	CHECK(FormatDate(again.start_date) == "2026-01-01");
	CHECK(FormatDate(again.finish_date) == "2026-02-01");
	// Still a rewatch along the way, not plain watching.
	auto along = catalist::StepProgress(again, 2, kToday);
	CHECK(along.status == WatchStatus::kRewatching);
	CHECK(along.rewatched == 0);
	auto over = catalist::StepProgress(along, 1, kToday);
	CHECK(over.status == WatchStatus::kCompleted);
	CHECK(over.progress == 3);
	CHECK(over.rewatched == 1);
	CHECK(FormatDate(over.finish_date) == "2026-02-01");
}

TEST_CASE("an entry's page is its AniList page") {
	CHECK(catalist::PageOf(MakeEntry().MalId(52991).AnilistId(154587)) ==
	      "https://anilist.co/anime/154587");
	CHECK(catalist::PageOf(MakeEntry().Type(catalist::MediaType::kManga).AnilistId(30002)) ==
	      "https://anilist.co/manga/30002");
	CHECK(catalist::PageOf(MakeEntry().MalId(52991)).empty());
}

TEST_CASE("setTitle and the date setters stamp the entry only when they change it") {
	using catalist::test::RequireDate;
	auto today = catalist::test::TestToday();
	catalist::Entry before = catalist::test::MakeEntry()
	                             .Title("Odd Taxi")
	                             .Status(WatchStatus::kWatching)
	                             .StartDate("2026-10-01")
	                             .UpdatedAt("2026-10-02");

	auto renamed = catalist::SetTitle(before, "ODDTAXI", today);
	CHECK(DisplayName(renamed) == "ODDTAXI");
	CHECK(renamed.title == "Odd Taxi");
	CHECK(renamed.updated_at == today);
	CHECK(catalist::SetTitle(before, "Odd Taxi", today).updated_at == before.updated_at);

	auto started = catalist::SetStartDate(before, RequireDate("2026-09-30"), today);
	CHECK(started.start_date == RequireDate("2026-09-30"));
	CHECK(started.updated_at == today);
	CHECK(catalist::SetStartDate(before, before.start_date, today).updated_at == before.updated_at);
	auto cleared = catalist::SetStartDate(before, std::nullopt, today);
	CHECK_FALSE(cleared.start_date);
	CHECK(cleared.updated_at == today);

	// A date corrects the record; the status is the person's to set.
	auto finished = catalist::SetFinishDate(before, RequireDate("2026-10-19"), today);
	CHECK(finished.finish_date == RequireDate("2026-10-19"));
	CHECK(finished.status == WatchStatus::kWatching);
	CHECK(finished.updated_at == today);
	CHECK(catalist::SetFinishDate(before, std::nullopt, today).updated_at == before.updated_at);
}

TEST_CASE("parseStatus handles UTF-8 safely and done finishes a rewatch") {
	CHECK_FALSE(ParseStatus("葬送のフリーレン").has_value());
	Entry again = MakeEntry()
	                  .Status(WatchStatus::kRewatching)
	                  .TotalUnits(12)
	                  .Progress(4)
	                  .StartDate("2026-01-01");
	auto done = catalist::SetStatus(again, WatchStatus::kCompleted, kToday);
	CHECK(done.status == WatchStatus::kCompleted);
	CHECK(done.progress == 12);
	CHECK(done.rewatched == 1);
}
