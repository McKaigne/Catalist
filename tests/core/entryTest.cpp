#include <doctest/doctest.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "core/entry.hpp"
#include "support/testSupport.hpp"

using catalist::parseStatus;
using catalist::watchStatus;

namespace {

void checkAllMapTo(const std::vector<std::string>& spellings, watchStatus expected) {
	for (const auto& spelling : spellings) {
		CAPTURE(spelling);
		CHECK(parseStatus(spelling) == expected);
	}
}

}  // namespace

TEST_CASE("parseStatus watching spellings") {
	checkAllMapTo({"watching", "Watching", "WATCHING", "watch", "reading", "Reading", "read", "1"},
	              watchStatus::kWatching);
}

TEST_CASE("parseStatus rewatching spellings") {
	checkAllMapTo({"rewatching", "Rewatching", "rereading", "Rereading", "REPEATING"},
	              watchStatus::kRewatching);
}

TEST_CASE("parseStatus completed spellings") {
	checkAllMapTo({"completed", "Completed", "complete", "2"}, watchStatus::kCompleted);
}

TEST_CASE("parseStatus on hold spellings") {
	checkAllMapTo({"on-hold", "On-Hold", "onhold", "on hold", "hold", "3"}, watchStatus::kOnHold);
}

TEST_CASE("parseStatus dropped spellings") {
	checkAllMapTo({"dropped", "Dropped", "drop", "4"}, watchStatus::kDropped);
}

TEST_CASE("parseStatus plan spellings") {
	checkAllMapTo({"plan to watch", "Plan to Watch", "Plan to Read", "planned", "plan",
	               "plan-to-watch", "plan-to-read", "5"},
	              watchStatus::kPlanToWatch);
}

TEST_CASE("parseStatus rejects unknown input") {
	for (const char* spelling : {"", "xyz", "0", "6", "ptw"}) {
		CAPTURE(spelling);
		CHECK_FALSE(parseStatus(spelling).has_value());
	}
}

TEST_CASE("statusToString round trips through parseStatus") {
	for (auto type : {catalist::mediaType::kAnime, catalist::mediaType::kManga}) {
		for (auto status : {watchStatus::kWatching, watchStatus::kCompleted, watchStatus::kOnHold,
		                    watchStatus::kDropped, watchStatus::kPlanToWatch}) {
			CHECK(parseStatus(catalist::statusToString(status, type)) == status);
		}
	}
}

using catalist::countField;
using catalist::entry;
using catalist::formatDate;
using catalist::isoDate;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::requireDate;

namespace {

const isoDate kToday = requireDate("2026-10-20");

entry watching(int progress, int total) {
	return makeEntry()
	    .id(1)
	    .title("Show")
	    .status(watchStatus::kWatching)
	    .progress(progress)
	    .totalUnits(total)
	    .startDate("2026-01-01");
}

}  // namespace

TEST_CASE("stepProgress clamps at zero and at the total") {
	CHECK(catalist::stepProgress(watching(5, 12), -9, kToday).progress == 0);

	auto finished = catalist::stepProgress(watching(5, 12), 40, kToday);
	CHECK(finished.progress == 12);
	CHECK(finished.status == watchStatus::kCompleted);
	CHECK(formatDate(finished.finishDate) == "2026-10-20");
	CHECK(formatDate(finished.updatedAt) == "2026-10-20");
}

TEST_CASE("stepProgress with an unknown total never completes") {
	auto open = catalist::stepProgress(watching(5, 0), 495, kToday);
	CHECK(open.progress == 500);
	CHECK(open.status == watchStatus::kWatching);
	CHECK_FALSE(open.finishDate.has_value());
}

TEST_CASE("stepProgress leaves a finished or empty entry exactly as it was") {
	// Pressing + on a completed title used to stamp today as its start and
	// finish date.
	entry done = makeEntry()
	                 .status(watchStatus::kCompleted)
	                 .progress(28)
	                 .totalUnits(28)
	                 .updatedAt("2024-01-01");
	auto same = catalist::stepProgress(done, 1, kToday);
	CHECK_FALSE(same.startDate.has_value());
	CHECK_FALSE(same.finishDate.has_value());
	CHECK(formatDate(same.updatedAt) == "2024-01-01");

	auto atZero = catalist::stepProgress(watching(0, 12), -1, kToday);
	CHECK_FALSE(atZero.updatedAt.has_value());
}

TEST_CASE("stepProgress forward starts a planned entry") {
	entry planned = makeEntry().status(watchStatus::kPlanToWatch).totalUnits(3);
	auto started = catalist::stepProgress(planned, 1, kToday);
	CHECK(started.progress == 1);
	CHECK(started.status == watchStatus::kWatching);
	CHECK(formatDate(started.startDate) == "2026-10-20");
	CHECK_FALSE(started.finishDate.has_value());
}

TEST_CASE("stepProgress forward to the total completes with a finish date") {
	auto finished = catalist::stepProgress(watching(11, 12), 1, kToday);
	CHECK(finished.status == watchStatus::kCompleted);
	CHECK(formatDate(finished.finishDate) == "2026-10-20");
	CHECK(formatDate(finished.startDate) == "2026-01-01");
}

TEST_CASE("stepProgress back below the total reopens a completed entry") {
	entry done = makeEntry()
	                 .status(watchStatus::kCompleted)
	                 .progress(12)
	                 .totalUnits(12)
	                 .finishDate("2026-05-05");
	auto reopened = catalist::stepProgress(done, -1, kToday);
	CHECK(reopened.progress == 11);
	CHECK(reopened.status == watchStatus::kWatching);
	CHECK_FALSE(reopened.finishDate.has_value());
}

TEST_CASE("stepProgress back stops at zero") {
	CHECK(catalist::stepProgress(watching(0, 12), -1, kToday).progress == 0);
}

TEST_CASE("setStatus to completed fills progress and finish date") {
	auto done = catalist::setStatus(watching(4, 12), watchStatus::kCompleted, kToday);
	CHECK(done.status == watchStatus::kCompleted);
	CHECK(done.progress == 12);
	CHECK(formatDate(done.finishDate) == "2026-10-20");
}

TEST_CASE("setStatus from completed back to watching clears the finish date") {
	entry done = makeEntry()
	                 .status(watchStatus::kCompleted)
	                 .finishDate("2026-05-05")
	                 .startDate("2026-01-01");
	auto resumed = catalist::setStatus(done, watchStatus::kWatching, kToday);
	CHECK(resumed.status == watchStatus::kWatching);
	CHECK_FALSE(resumed.finishDate.has_value());
}

TEST_CASE("setStatus to on hold keeps an existing finish date") {
	entry done = makeEntry().status(watchStatus::kCompleted).finishDate("2026-05-05");
	auto onHold = catalist::setStatus(done, watchStatus::kOnHold, kToday);
	CHECK(onHold.status == watchStatus::kOnHold);
	CHECK(formatDate(onHold.finishDate) == "2026-05-05");
}

TEST_CASE("setScore stays within zero and ten") {
	CHECK(catalist::setScore(makeEntry().score(9), 12, kToday).score == 10);
	CHECK(catalist::setScore(makeEntry().score(1), -2, kToday).score == 0);
	CHECK(formatDate(catalist::setScore(makeEntry(), 1, kToday).updatedAt) == "2026-10-20");
	// The score it has already: nothing changes, not even the date.
	CHECK_FALSE(catalist::setScore(makeEntry().score(10), 11, kToday).updatedAt.has_value());
}

TEST_CASE("setCount types progress in like a long step") {
	entry planned = makeEntry().status(watchStatus::kPlanToWatch).totalUnits(24);
	auto started = catalist::setCount(planned, countField::kProgress, 10, kToday);
	CHECK(started.progress == 10);
	CHECK(started.status == watchStatus::kWatching);
	CHECK(formatDate(started.startDate) == "2026-10-20");

	auto done = catalist::setCount(started, countField::kProgress, 99, kToday);
	CHECK(done.progress == 24);
	CHECK(done.status == watchStatus::kCompleted);

	auto reopened = catalist::setCount(done, countField::kProgress, 3, kToday);
	CHECK(reopened.progress == 3);
	CHECK(reopened.status == watchStatus::kWatching);
	CHECK_FALSE(reopened.finishDate.has_value());
}

TEST_CASE("setCount sets totals, volumes and rewatches within bounds") {
	entry show = watching(10, 0);
	auto withTotal = catalist::setCount(show, countField::kTotalUnits, 24, kToday);
	CHECK(withTotal.totalUnits == 24);
	CHECK(withTotal.progress == 10);
	CHECK(formatDate(withTotal.updatedAt) == "2026-10-20");
	// A total below the progress lowers the progress; 0 means unknown.
	CHECK(catalist::setCount(show, countField::kTotalUnits, 6, kToday).progress == 6);
	CHECK(catalist::setCount(withTotal, countField::kTotalUnits, 0, kToday).progress == 10);
	CHECK(catalist::setCount(show, countField::kTotalUnits, -5, kToday).totalUnits == 0);

	entry book = makeEntry().type(catalist::mediaType::kManga).totalVolumes(11);
	CHECK(catalist::setCount(book, countField::kVolumes, 4, kToday).volumes == 4);
	CHECK(catalist::setCount(book, countField::kVolumes, 40, kToday).volumes == 11);
	entry open = makeEntry().type(catalist::mediaType::kManga);
	CHECK(catalist::setCount(open, countField::kVolumes, 40, kToday).volumes == 40);

	CHECK(catalist::setCount(show, countField::kRewatched, 2, kToday).rewatched == 2);
	CHECK(catalist::setCount(show, countField::kRewatched, -1, kToday).rewatched == 0);
	CHECK_FALSE(catalist::setCount(show, countField::kRewatched, 0, kToday).updatedAt.has_value());
}

TEST_CASE("countOf reads each count") {
	entry e = makeEntry().progress(1).totalUnits(2).volumes(3).rewatched(4);
	CHECK(catalist::countOf(e, countField::kProgress) == 1);
	CHECK(catalist::countOf(e, countField::kTotalUnits) == 2);
	CHECK(catalist::countOf(e, countField::kVolumes) == 3);
	CHECK(catalist::countOf(e, countField::kRewatched) == 4);
}

TEST_CASE("mergeImported refreshes totals and fills only what the user left empty") {
	entry mine = makeEntry().malId(5).score(8).progress(3).totalUnits(0).startDate("2026-01-01");
	entry imported = makeEntry()
	                     .malId(5)
	                     .score(2)
	                     .progress(9)
	                     .totalUnits(12)
	                     .totalVolumes(3)
	                     .startDate("2020-01-01")
	                     .finishDate("2020-02-02");
	auto merged = catalist::mergeImported(mine, imported);
	CHECK(merged.score == 8);
	CHECK(merged.progress == 3);
	CHECK(merged.totalUnits == 12);
	CHECK(merged.totalVolumes == 3);
	CHECK(formatDate(merged.startDate) == "2026-01-01");
	CHECK(formatDate(merged.finishDate) == "2020-02-02");
}

TEST_CASE("newReleases counts only for titles being followed") {
	auto withStatus = [](watchStatus status) {
		return makeEntry().status(status).progress(5).released(12).build();
	};
	CHECK(catalist::newReleases(withStatus(watchStatus::kWatching)) == 7);
	CHECK(catalist::newReleases(withStatus(watchStatus::kOnHold)) == 7);
	CHECK(catalist::newReleases(withStatus(watchStatus::kCompleted)) == 0);
	CHECK(catalist::newReleases(withStatus(watchStatus::kDropped)) == 0);
	CHECK(catalist::newReleases(withStatus(watchStatus::kPlanToWatch)) == 0);
	// Ahead of what is known to be out, or nothing known: no new releases.
	CHECK(catalist::newReleases(makeEntry().progress(20).released(12).build()) == 0);
	CHECK(catalist::newReleases(makeEntry().progress(3).build()) == 0);
}

TEST_CASE("applyWebFacts keeps known values the check did not have") {
	auto tracked =
	    makeEntry().totalUnits(24).totalVolumes(3).released(10).updatedAt("2026-01-01").build();
	auto refreshed = catalist::applyWebFacts(tracked, {0, 0, 0}, kToday);
	CHECK(refreshed.totalUnits == 24);
	CHECK(refreshed.totalVolumes == 3);
	CHECK(refreshed.released == 10);
	CHECK(formatDate(refreshed.checkedAt) == "2026-10-20");
	CHECK(formatDate(refreshed.updatedAt) == "2026-01-01");
	auto finished = catalist::applyWebFacts(tracked, {26, 4, 26}, kToday);
	CHECK(finished.totalUnits == 26);
	CHECK(finished.totalVolumes == 4);
	CHECK(finished.released == 26);
}

TEST_CASE("setStatus fills the dates like the cycle and ignores no change") {
	entry planned = makeEntry().status(watchStatus::kPlanToWatch).totalUnits(12).progress(3);
	auto done = catalist::setStatus(planned, watchStatus::kCompleted, kToday);
	CHECK(done.status == watchStatus::kCompleted);
	CHECK(done.progress == 12);
	CHECK(formatDate(done.startDate) == "2026-10-20");
	CHECK(formatDate(done.finishDate) == "2026-10-20");
	auto same = catalist::setStatus(planned, watchStatus::kPlanToWatch, kToday);
	CHECK_FALSE(same.updatedAt.has_value());
	CHECK(catalist::setScore(planned, 14, kToday).score == 10);
	CHECK(catalist::setScore(planned, 7, kToday).score == 7);
}

TEST_CASE("a rewatch starts over, keeps the first dates, and counts once it is done") {
	entry done = makeEntry()
	                 .status(watchStatus::kCompleted)
	                 .totalUnits(3)
	                 .progress(3)
	                 .startDate("2026-01-01")
	                 .finishDate("2026-02-01");
	auto again = catalist::setStatus(done, watchStatus::kRewatching, kToday);
	CHECK(again.status == watchStatus::kRewatching);
	CHECK(again.progress == 0);
	CHECK(formatDate(again.startDate) == "2026-01-01");
	CHECK(formatDate(again.finishDate) == "2026-02-01");
	// Still a rewatch along the way, not plain watching.
	auto along = catalist::stepProgress(again, 2, kToday);
	CHECK(along.status == watchStatus::kRewatching);
	CHECK(along.rewatched == 0);
	auto over = catalist::stepProgress(along, 1, kToday);
	CHECK(over.status == watchStatus::kCompleted);
	CHECK(over.progress == 3);
	CHECK(over.rewatched == 1);
	CHECK(formatDate(over.finishDate) == "2026-10-20");
}

TEST_CASE("entries show the chosen provider's id and page") {
	entry frieren = makeEntry().malId(52991).anilistId(154587);
	CHECK(catalist::idOn(frieren, catalist::provider::kAniList) == 154587);
	CHECK(catalist::pageOn(frieren, catalist::provider::kAniList) ==
	      "https://anilist.co/anime/154587");
	CHECK(catalist::pageOn(frieren, catalist::provider::kMyAnimeList) ==
	      "https://myanimelist.net/anime/52991");
	CHECK(catalist::pageOn(makeEntry(), catalist::provider::kAniList).empty());
	CHECK(catalist::parseProvider("MAL") == catalist::provider::kMyAnimeList);
	CHECK(catalist::parseProvider("AniList") == catalist::provider::kAniList);
	CHECK(catalist::parseProvider("Kitsu") == catalist::provider::kKitsu);
	CHECK_FALSE(catalist::parseProvider("shikimori").has_value());
	// Kitsu's id is the one Kitsu gave; without it there is no page there.
	CHECK(catalist::pageOn(frieren, catalist::provider::kKitsu).empty());
	entry onKitsu = makeEntry().kitsuId(46474);
	CHECK(catalist::idOn(onKitsu, catalist::provider::kKitsu) == 46474);
	CHECK(catalist::pageOn(onKitsu, catalist::provider::kKitsu) == "https://kitsu.app/anime/46474");
	CHECK(catalist::providerKey(catalist::provider::kKitsu) == "kitsu");
	CHECK(catalist::providerKey(catalist::provider::kMyAnimeList) == "mal");
}

TEST_CASE("applyWebFacts fills in an id the entry lacked, never replaces one") {
	entry known = makeEntry().malId(2);
	catalist::webFacts facts;
	facts.anilistId = 30002;
	facts.malId = 999;
	auto filled = catalist::applyWebFacts(known, facts, kToday);
	CHECK(filled.anilistId == 30002);
	CHECK(filled.malId == 2);
}

TEST_CASE("setTitle and the date setters stamp the entry only when they change it") {
	using catalist::test::requireDate;
	auto today = catalist::test::testToday();
	catalist::entry before = catalist::test::makeEntry()
	                             .title("Odd Taxi")
	                             .status(watchStatus::kWatching)
	                             .startDate("2026-10-01")
	                             .updatedAt("2026-10-02");

	auto renamed = catalist::setTitle(before, "ODDTAXI", today);
	CHECK(renamed.title == "ODDTAXI");
	CHECK(renamed.updatedAt == today);
	CHECK(catalist::setTitle(before, "Odd Taxi", today).updatedAt == before.updatedAt);

	auto started = catalist::setStartDate(before, requireDate("2026-09-30"), today);
	CHECK(started.startDate == requireDate("2026-09-30"));
	CHECK(started.updatedAt == today);
	CHECK(catalist::setStartDate(before, before.startDate, today).updatedAt == before.updatedAt);
	auto cleared = catalist::setStartDate(before, std::nullopt, today);
	CHECK_FALSE(cleared.startDate);
	CHECK(cleared.updatedAt == today);

	// A date corrects the record; the status is the person's to set.
	auto finished = catalist::setFinishDate(before, requireDate("2026-10-19"), today);
	CHECK(finished.finishDate == requireDate("2026-10-19"));
	CHECK(finished.status == watchStatus::kWatching);
	CHECK(finished.updatedAt == today);
	CHECK(catalist::setFinishDate(before, std::nullopt, today).updatedAt == before.updatedAt);
}
