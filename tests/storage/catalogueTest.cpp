#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>

#include "storage/catalogue.hpp"
#include "storage/csvCodec.hpp"
#include "support/testSupport.hpp"
#include "system.hpp"

using catalist::formatDate;
using catalist::mediaType;
using catalist::watchStatus;
namespace csvCodec = catalist::csvCodec;

using catalist::catalogue;
using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::tempDirFixture;

TEST_CASE_FIXTURE(tempDirFixture, "catalogue round trip keeps quotes, commas and dates") {
	catalogue db(tempDir());
	catalist::entry original = makeEntry()
	                               .malId(9901)
	                               .title("Fate/stay night: Heavens Feel, Part \"1\"")
	                               .status(watchStatus::kCompleted)
	                               .score(10)
	                               .progress(3)
	                               .totalUnits(3)
	                               .startDate("2017-10-14")
	                               .finishDate("2020-08-15");
	int id = db.addEntry(original);
	REQUIRE(db.saveAll());

	catalogue reloaded(tempDir());
	REQUIRE(reloaded.loadAll().value());
	auto loaded = reloaded.getById(mediaType::kAnime, id);
	REQUIRE(loaded.has_value());
	CHECK(loaded->title == original.title);
	CHECK(loaded->score == 10);
	CHECK(loaded->progress == 3);
	CHECK(loaded->startDate == original.startDate);
	CHECK(loaded->finishDate == original.finishDate);
	CHECK_FALSE(loaded->updatedAt.has_value());
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "catalogue gives each new title the next free id per media type") {
	catalogue db(tempDir() / "data");
	CHECK(db.addEntry(makeEntry().title("One")) == 1);
	CHECK(db.addEntry(makeEntry().title("Two").malId(5)) == 2);
	// An id the caller sets is not kept: ids are only row numbers.
	CHECK(db.addEntry(makeEntry().id(500).title("Three")) == 3);
	CHECK(db.addEntry(makeEntry().title("Manga").type(mediaType::kManga)) == 1);

	REQUIRE(db.removeEntry(mediaType::kAnime, 2));
	CHECK(db.addEntry(makeEntry().title("Four")) == 4);
	REQUIRE(db.removeEntry(mediaType::kAnime, 4));
	REQUIRE(db.removeEntry(mediaType::kAnime, 3));
	CHECK(db.addEntry(makeEntry().title("Five")) == 2);
	CHECK_FALSE(db.removeEntry(mediaType::kAnime, 99));
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "catalogue continues numbering after the ids in the stored file") {
	{
		catalogue first(tempDir() / "data");
		first.addEntry(makeEntry().title("Loaded"));
		REQUIRE(first.saveAll());
		catalist::store::database raw;
		REQUIRE(raw.open(first.getStorePath()));
		REQUIRE(raw.run("UPDATE entry SET id = 40"));
	}
	catalogue db(tempDir() / "data");
	REQUIRE(db.loadAll().value());
	CHECK(db.getById(mediaType::kAnime, 40)->title == "Loaded");
	CHECK(db.addEntry(makeEntry().title("Next")) == 41);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue getById and findByIds look in one media type only") {
	catalogue db(tempDir() / "data");
	int anime = db.addEntry(makeEntry().title("Anime").malId(1).anilistId(11));
	db.addEntry(makeEntry().title("Manga").malId(1).type(mediaType::kManga));
	CHECK(db.getById(mediaType::kAnime, anime)->title == "Anime");
	CHECK(db.findByIds(mediaType::kManga, 0, 1)->title == "Manga");
	CHECK(db.findByIds(mediaType::kAnime, 11, 0)->title == "Anime");
	CHECK(db.findByIds(mediaType::kAnime, 99, 1)->title == "Anime");
	CHECK_FALSE(db.findByIds(mediaType::kAnime, 0, 2).has_value());
	CHECK_FALSE(db.findByIds(mediaType::kAnime, 0, 0).has_value());
	CHECK_FALSE(db.getById(mediaType::kAnime, 99).has_value());
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue keeps the provider") {
	catalogue db(tempDir() / "data");
	REQUIRE(db.loadAll().has_value());
	CHECK(db.getProvider() == catalist::provider::kAniList);
	db.setProvider(catalist::provider::kMyAnimeList);
	// Only the settings are written: a title added but not saved stays out.
	db.addEntry(makeEntry().title("Not saved yet"));
	REQUIRE(db.saveSettings());

	catalogue reread(tempDir() / "data");
	REQUIRE(reread.loadAll().value());
	CHECK(reread.getProvider() == catalist::provider::kMyAnimeList);
	CHECK(reread.getEntries(mediaType::kAnime).empty());

	// saveAll writes it too, with the lists.
	reread.setProvider(catalist::provider::kAniList);
	REQUIRE(reread.saveAll());
	catalogue last(tempDir() / "data");
	REQUIRE(last.loadAll().value());
	CHECK(last.getProvider() == catalist::provider::kAniList);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue leaves the settings of Catalist-TUI alone") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("First"));
	REQUIRE(db.saveAll());
	{
		catalist::store::database raw;
		REQUIRE(raw.open(db.getStorePath()));
		REQUIRE(raw.run("INSERT INTO setting VALUES ('linked', '2026-10-19')"));
	}
	catalogue again(tempDir() / "data");
	REQUIRE(again.loadAll().value());
	again.setProvider(catalist::provider::kMyAnimeList);
	REQUIRE(again.saveSettings());

	catalist::store::database raw;
	REQUIRE(raw.open(db.getStorePath()));
	auto linked = raw.prepare("SELECT value FROM setting WHERE key = 'linked'");
	REQUIRE(linked.step());
	CHECK(linked.textAt(0) == "2026-10-19");
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue merges an added title known by either id") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Frieren").malId(52991));
	int merged = db.addEntry(makeEntry().title("Frieren").malId(52991).anilistId(154587));
	CHECK(db.getEntries(mediaType::kAnime).size() == 1);
	CHECK(db.getById(mediaType::kAnime, merged)->anilistId == 154587);
	db.addEntry(makeEntry().title("Frieren").anilistId(154587));
	CHECK(db.getEntries(mediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue updateEntry replaces the entry with the same id") {
	catalogue db(tempDir() / "data");
	int id = db.addEntry(makeEntry().title("Before"));
	auto changed = *db.getById(mediaType::kAnime, id);
	changed.title = "After";
	CHECK(db.updateEntry(changed));
	CHECK(db.getById(mediaType::kAnime, id)->title == "After");
	changed.id = 99;
	CHECK_FALSE(db.updateEntry(changed));
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue loadAll says whether anything was stored") {
	catalogue db(tempDir() / "data");
	auto loaded = db.loadAll();
	REQUIRE(loaded.has_value());
	CHECK_FALSE(loaded.value());
	// Looking at an empty catalogue creates nothing.
	CHECK_FALSE(std::filesystem::exists(tempDir() / "data"));

	db.addEntry(makeEntry().title("First"));
	REQUIRE(db.saveAll());
	CHECK(db.loadAll().value());
	catalogue again(tempDir() / "data");
	CHECK(again.loadAll().value());
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "addEntry re-add keeps user fields and refreshes upstream metadata") {
	catalogue db(tempDir() / "data");
	int id = db.addEntry(makeEntry()
	                         .malId(52991)
	                         .title("Sousou no Frieren")
	                         .score(8)
	                         .progress(5)
	                         .totalUnits(0)
	                         .startDate("2023-09-29"));
	int mergedId = db.addEntry(makeEntry()
	                               .malId(52991)
	                               .title("Frieren (remote)")
	                               .score(3)
	                               .progress(20)
	                               .totalUnits(28)
	                               .totalVolumes(4)
	                               .startDate("1999-01-01"));

	CHECK(mergedId == id);
	REQUIRE(db.getEntries(mediaType::kAnime).size() == 1);
	auto merged = *db.getById(mediaType::kAnime, id);
	CHECK(merged.score == 8);
	CHECK(merged.progress == 5);
	CHECK(catalist::formatDate(merged.startDate) == "2023-09-29");
	CHECK(merged.title == "Sousou no Frieren");
	CHECK(merged.totalUnits == 28);
	CHECK(merged.totalVolumes == 4);
}

TEST_CASE_FIXTURE(tempDirFixture, "addEntry re-add backfills empty user fields") {
	catalogue db(tempDir() / "data");
	int id = db.addEntry(makeEntry().malId(100).title("Blank"));
	db.addEntry(
	    makeEntry().malId(100).title("Blank").score(7).progress(3).finishDate("2024-01-01"));
	auto merged = *db.getById(mediaType::kAnime, id);
	CHECK(merged.score == 7);
	CHECK(merged.progress == 3);
	CHECK(catalist::formatDate(merged.finishDate) == "2024-01-01");
}

TEST_CASE_FIXTURE(tempDirFixture, "addEntry matches by MyAnimeList id, never by title") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().malId(777).title("Local"));
	db.addEntry(makeEntry().title("Remote").totalUnits(12).malId(777));
	db.addEntry(makeEntry().title("Same title"));
	db.addEntry(makeEntry().title("Same title"));
	db.addEntry(makeEntry().title("Local").malId(778));

	const auto& anime = db.getEntries(mediaType::kAnime);
	REQUIRE(anime.size() == 4);
	CHECK(anime[0].title == "Local");
	CHECK(anime[0].totalUnits == 12);
	CHECK(std::count_if(anime.begin(), anime.end(),
	                    [](const auto& e) { return e.title == "Same title"; }) == 2);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue saves both lists and reloads them") {
	catalogue db(tempDir() / "data");
	int id = db.addEntry(makeEntry()
	                         .title("Kept, \"quoted\"")
	                         .malId(7)
	                         .anilistId(70)
	                         .status(watchStatus::kOnHold)
	                         .score(6)
	                         .progress(3)
	                         .totalUnits(12)
	                         .rewatched(2)
	                         .released(5)
	                         .startDate("2026-01-02")
	                         .finishDate("2026-02-03")
	                         .updatedAt("2026-03-04")
	                         .checkedAt("2026-04-05"));
	db.addEntry(makeEntry().title("Paper").type(mediaType::kManga).volumes(3).totalVolumes(9));
	REQUIRE(db.saveAll());
	CHECK(std::filesystem::exists(tempDir() / "data/catalist.db"));
	// One file holds everything; no CSV or settings file is written.
	CHECK_FALSE(std::filesystem::exists(tempDir() / "data/anime.csv"));
	CHECK_FALSE(std::filesystem::exists(tempDir() / "data/settings"));

	catalogue reloaded(tempDir() / "data");
	REQUIRE(reloaded.loadAll().value());
	CHECK(csvCodec::encodeEntry(*reloaded.getById(mediaType::kAnime, id)) ==
	      "1,7,anime,\"Kept, \"\"quoted\"\"\",On-Hold,6,3,12,0,0,2,2026-01-02,2026-02-03,"
	      "2026-03-04,5,2026-04-05,70");
	CHECK(csvCodec::encodeEntry(reloaded.getEntries(mediaType::kManga)[0]) ==
	      "1,0,manga,Paper,Reading,0,0,0,3,9,0,,,,0,,0");
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue writes only the rows it changed") {
	auto dir = tempDir() / "data";
	{
		catalogue db(dir);
		db.addEntry(makeEntry().title("One"));
		db.addEntry(makeEntry().title("Two"));
		db.addEntry(makeEntry().title("Three"));
		REQUIRE(db.saveAll());
	}
	// Two programs have the lists open; each changes another title.
	catalogue first(dir);
	catalogue second(dir);
	REQUIRE(first.loadAll().value());
	REQUIRE(second.loadAll().value());
	auto one = *first.getById(mediaType::kAnime, 1);
	one.score = 9;
	REQUIRE(first.updateEntry(one));
	REQUIRE(first.saveAll());
	auto two = *second.getById(mediaType::kAnime, 2);
	two.progress = 4;
	REQUIRE(second.updateEntry(two));
	REQUIRE(second.removeEntry(mediaType::kAnime, 3));
	REQUIRE(second.saveAll());

	catalogue after(dir);
	REQUIRE(after.loadAll().value());
	CHECK(after.getById(mediaType::kAnime, 1)->score == 9);
	CHECK(after.getById(mediaType::kAnime, 2)->progress == 4);
	CHECK_FALSE(after.getById(mediaType::kAnime, 3).has_value());
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue beginWrite holds the catalogue until the save") {
	auto dir = tempDir() / "data";
	{
		// A command that gives up before saving leaves nothing behind.
		catalogue db(dir);
		REQUIRE(db.beginWrite().has_value());
		REQUIRE(db.loadAll().has_value());
		db.addEntry(makeEntry().title("Dropped on the way"));
	}
	{
		catalogue db(dir);
		REQUIRE(db.beginWrite().has_value());
		REQUIRE(db.loadAll().has_value());
		CHECK(db.getEntries(mediaType::kAnime).empty());
		db.addEntry(makeEntry().title("Kept"));
		REQUIRE(db.saveAll());
		// The save ended the write: another program can write again.
		catalogue other(dir);
		REQUIRE(other.beginWrite().has_value());
	}
	catalogue after(dir);
	REQUIRE(after.loadAll().value());
	REQUIRE(after.getEntries(mediaType::kAnime).size() == 1);
	CHECK(after.getEntries(mediaType::kAnime)[0].title == "Kept");
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue peek reads the lists and leaves nothing behind") {
	auto dir = tempDir() / "data";
	// Nothing stored: not even the directory is made.
	catalogue nothing(dir);
	CHECK_FALSE(nothing.peek());
	CHECK_FALSE(std::filesystem::exists(dir));

	{
		catalogue db(dir);
		db.addEntry(makeEntry().title("Odd Taxi"));
		REQUIRE(db.saveAll());
	}
	catalogue db(dir);
	REQUIRE(db.peek());
	REQUIRE(db.getEntries(mediaType::kAnime).size() == 1);
	CHECK(db.getEntries(mediaType::kAnime)[0].title == "Odd Taxi");
	// The file was closed again: another program writes without waiting.
	catalogue writer(dir);
	CHECK(writer.beginWrite().has_value());

	catalist::test::writeText(tempDir() / "other/catalist.db", "this is not a database");
	catalogue broken(tempDir() / "other");
	CHECK_FALSE(broken.peek());
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue reports a failed write and a file it cannot open") {
	catalogue db(tempDir() / "data");
	catalist::test::makeUnwritable(tempDir() / "data/catalist.db");
	db.addEntry(makeEntry().title("Nowhere to go"));
	CHECK_FALSE(db.saveAll());
	CHECK_FALSE(db.saveSettings());
	CHECK_FALSE(db.beginWrite().has_value());
	auto loaded = db.loadAll();
	REQUIRE_FALSE(loaded.has_value());
	CHECK(loaded.error().find("cannot open " + (tempDir() / "data/catalist.db").string() + ": ") ==
	      0);

	catalist::test::writeText(tempDir() / "other/catalist.db", "this is not a database");
	catalogue broken(tempDir() / "other");
	CHECK_FALSE(broken.loadAll().has_value());
	CHECK(catalist::test::readText(tempDir() / "other/catalist.db") == "this is not a database");
}

namespace {

catalist::txNote noteOf(const std::string& label) {
	return {label, "2026-10-20 21:30:00"};
}

// Frieren (a:1, completed) and Odd Taxi (a:2, 5 of 13) and Berserk (m:1),
// saved without a step in the history.
void seedForUndo(catalogue& db) {
	db.addEntry(makeEntry().title("Sousou no Frieren").malId(52991).anilistId(154587).score(10));
	db.addEntry(
	    makeEntry().title("Odd Taxi").malId(44265).anilistId(128547).progress(5).totalUnits(13));
	db.addEntry(makeEntry().title("Berserk").type(mediaType::kManga).malId(2).progress(364));
	REQUIRE(db.saveAll());
}

// Sets the progress of an anime and saves it as one step.
void watch(catalogue& db, int id, int progress, const catalist::txNote& note) {
	auto title = *db.getById(mediaType::kAnime, id);
	title.progress = progress;
	REQUIRE(db.updateEntry(title));
	REQUIRE(db.saveAll(note));
}

int progressOf(const catalogue& db, int id) {
	return db.getById(mediaType::kAnime, id)->progress;
}

}  // namespace

TEST_CASE_FIXTURE(tempDirFixture, "catalogue enters a save with a note in the undo history") {
	catalogue db(tempDir() / "data");
	CHECK(db.history(0).value().empty());
	seedForUndo(db);
	CHECK(db.history(0).value().empty());

	watch(db, 2, 6, noteOf("+ taxi"));
	auto berserk = *db.getById(mediaType::kManga, 1);
	berserk.score = 10;
	REQUIRE(db.updateEntry(berserk));
	watch(db, 1, 3, noteOf("set a:1 progress=3"));

	auto steps = db.history(0).value();
	REQUIRE(steps.size() == 2);
	// Newest first.
	CHECK(steps[0].id == 2);
	CHECK(steps[0].label == "set a:1 progress=3");
	CHECK(steps[0].stamp == "2026-10-20 21:30:00");
	CHECK(steps[0].changes == 2);
	CHECK_FALSE(steps[0].undone);
	CHECK(steps[1].label == "+ taxi");
	CHECK(steps[1].changes == 1);
	CHECK(db.history(1).value().size() == 1);
	CHECK(db.history(1).value()[0].id == 2);

	// Nothing the person sets has changed: no step.
	REQUIRE(db.saveAll(noteOf("nothing")));
	auto taxi = *db.getById(mediaType::kAnime, 2);
	taxi.released = 13;
	taxi.checkedAt = catalist::test::requireDate("2026-10-20");
	REQUIRE(db.updateEntry(taxi));
	REQUIRE(db.saveAll(noteOf("web")));
	CHECK(db.history(0).value().size() == 2);

	// The history is in the file: another program reads the same.
	catalogue other(tempDir() / "data");
	REQUIRE(other.loadAll().value());
	CHECK(other.getById(mediaType::kAnime, 2)->released == 13);
	CHECK(other.history(0).value().size() == 2);
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "catalogue undo takes back the newest step, redo makes it again") {
	catalogue db(tempDir() / "data");
	seedForUndo(db);
	watch(db, 2, 6, noteOf("first"));
	watch(db, 2, 7, noteOf("second"));

	auto undone = db.undo(1, false);
	REQUIRE(undone.has_value());
	REQUIRE(undone.value().steps.size() == 1);
	CHECK(undone.value().steps[0].label == "second");
	REQUIRE(undone.value().changes.size() == 1);
	CHECK(undone.value().changes[0].before->progress == 7);
	CHECK(undone.value().changes[0].after->progress == 6);
	CHECK(progressOf(db, 2) == 6);
	auto steps = db.history(0).value();
	CHECK(steps[0].undone);
	CHECK_FALSE(steps[1].undone);
	{
		catalogue saved(tempDir() / "data");
		REQUIRE(saved.loadAll().value());
		CHECK(progressOf(saved, 2) == 6);
	}

	REQUIRE(db.undo(1, false).has_value());
	CHECK(progressOf(db, 2) == 5);
	auto nothing = db.undo(1, false);
	REQUIRE(nothing.has_value());
	CHECK(nothing.value().steps.empty());
	CHECK(nothing.value().changes.empty());

	// The step undone last is made again first.
	auto redone = db.redo(1, false);
	REQUIRE(redone.has_value());
	REQUIRE(redone.value().steps.size() == 1);
	CHECK(redone.value().steps[0].label == "first");
	CHECK(progressOf(db, 2) == 6);
	REQUIRE(db.redo(5, false).has_value());
	CHECK(progressOf(db, 2) == 7);
	CHECK(db.redo(1, false).value().steps.empty());
	steps = db.history(0).value();
	for (const auto& step : steps)
		CHECK_FALSE(step.undone);

	// Several at once, newest first; more than there are is all of them.
	auto both = db.undo(9, false);
	REQUIRE(both.has_value());
	REQUIRE(both.value().steps.size() == 2);
	CHECK(both.value().steps[0].label == "second");
	CHECK(both.value().steps[1].label == "first");
	REQUIRE(both.value().changes.size() == 1);
	CHECK(both.value().changes[0].before->progress == 7);
	CHECK(both.value().changes[0].after->progress == 5);
	catalogue saved(tempDir() / "data");
	REQUIRE(saved.loadAll().value());
	CHECK(progressOf(saved, 2) == 5);
	CHECK(saved.redo(2, false).value().steps.size() == 2);
	CHECK(progressOf(saved, 2) == 7);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue undo keeps what the web filled in since") {
	catalogue db(tempDir() / "data");
	seedForUndo(db);
	watch(db, 2, 6, noteOf("+ taxi"));
	auto taxi = *db.getById(mediaType::kAnime, 2);
	taxi.released = 13;
	taxi.score = 9;
	REQUIRE(db.updateEntry(taxi));
	REQUIRE(db.saveAll());

	REQUIRE(db.undo(1, false).has_value());
	taxi = *db.getById(mediaType::kAnime, 2);
	CHECK(taxi.progress == 5);
	CHECK(taxi.released == 13);
	CHECK(taxi.score == 9);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue undo changes nothing when a title has changed since") {
	catalogue db(tempDir() / "data");
	seedForUndo(db);
	watch(db, 2, 6, noteOf("older"));
	watch(db, 1, 4, noteOf("newer"));
	// Changed outside the history, as a web check changes a total.
	auto taxi = *db.getById(mediaType::kAnime, 2);
	taxi.progress = 9;
	REQUIRE(db.updateEntry(taxi));
	REQUIRE(db.saveAll());

	// The newer step could be taken back, the older one not: neither is.
	auto refused = db.undo(2, false);
	REQUIRE_FALSE(refused.has_value());
	CHECK(refused.error() ==
	      "cannot undo 'older' (2026-10-20 21:30:00): Odd Taxi (a:2) has changed since: "
	      "progress is now 9, not 6");
	CHECK(progressOf(db, 1) == 4);
	CHECK(progressOf(db, 2) == 9);
	auto steps = db.history(0).value();
	for (const auto& step : steps)
		CHECK_FALSE(step.undone);
	{
		catalogue saved(tempDir() / "data");
		REQUIRE(saved.loadAll().value());
		CHECK(progressOf(saved, 1) == 4);
		CHECK(progressOf(saved, 2) == 9);
	}

	auto forced = db.undo(2, true);
	REQUIRE(forced.has_value());
	CHECK(forced.value().steps.size() == 2);
	CHECK(progressOf(db, 1) == 0);
	CHECK(progressOf(db, 2) == 5);

	// Redo checks the same way.
	taxi = *db.getById(mediaType::kAnime, 2);
	taxi.progress = 1;
	REQUIRE(db.updateEntry(taxi));
	REQUIRE(db.saveAll());
	auto again = db.redo(1, false);
	REQUIRE_FALSE(again.has_value());
	CHECK(again.error() ==
	      "cannot redo 'older' (2026-10-20 21:30:00): Odd Taxi (a:2) has changed since: "
	      "progress is now 1, not 5");
	CHECK(progressOf(db, 2) == 1);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue undo brings a removed title back under its ref") {
	catalogue db(tempDir() / "data");
	seedForUndo(db);
	db.addEntry(makeEntry().title("Mushishi"));
	REQUIRE(db.saveAll());
	auto taxi = *db.getById(mediaType::kAnime, 2);
	REQUIRE(db.removeEntry(mediaType::kAnime, 2));
	REQUIRE(db.saveAll(noteOf("rm taxi")));

	// In its place: the lists are kept in the order of their ids.
	CHECK_FALSE(db.restoreEntry(*db.getById(mediaType::kAnime, 3)));
	REQUIRE(db.undo(1, false).has_value());
	auto anime = db.getEntries(mediaType::kAnime);
	REQUIRE(anime.size() == 3);
	CHECK(anime[1].id == 2);
	CHECK(csvCodec::encodeEntry(anime[1]) == csvCodec::encodeEntry(taxi));
	CHECK(anime[2].title == "Mushishi");
	{
		catalogue saved(tempDir() / "data");
		REQUIRE(saved.loadAll().value());
		CHECK(saved.getById(mediaType::kAnime, 2)->title == "Odd Taxi");
	}
	// Redo removes it again; undo of an addition removes what was added.
	REQUIRE(db.redo(1, false).has_value());
	CHECK_FALSE(db.getById(mediaType::kAnime, 2));
	int added = db.addEntry(makeEntry().title("Dandadan").anilistId(171018));
	REQUIRE(db.saveAll(noteOf("add dandadan")));
	REQUIRE(db.undo(1, false).has_value());
	CHECK_FALSE(db.getById(mediaType::kAnime, added));
	REQUIRE(db.redo(1, false).has_value());
	CHECK(db.getById(mediaType::kAnime, added)->title == "Dandadan");
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue undo cannot bring a title back to a ref in use") {
	catalogue db(tempDir() / "data");
	seedForUndo(db);
	REQUIRE(db.removeEntry(mediaType::kAnime, 2));
	REQUIRE(db.saveAll(noteOf("rm taxi")));
	// The highest id was freed, and the next title added got it.
	CHECK(db.addEntry(makeEntry().title("Mushishi")) == 2);
	REQUIRE(db.saveAll());

	for (bool force : {false, true}) {
		auto refused = db.undo(1, force);
		REQUIRE_FALSE(refused.has_value());
		CHECK(refused.error() ==
		      "cannot undo 'rm taxi' (2026-10-20 21:30:00): Odd Taxi (a:2) cannot come back: "
		      "its ref is now that of Mushishi");
	}
	CHECK(db.getById(mediaType::kAnime, 2)->title == "Mushishi");
	CHECK_FALSE(db.history(0).value()[0].undone);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue forgets what was undone when new work is saved") {
	catalogue db(tempDir() / "data");
	seedForUndo(db);
	watch(db, 2, 6, noteOf("first"));
	watch(db, 2, 7, noteOf("second"));
	REQUIRE(db.undo(1, false).has_value());
	// A save outside the history leaves what can be redone.
	auto frieren = *db.getById(mediaType::kAnime, 1);
	frieren.released = 28;
	REQUIRE(db.updateEntry(frieren));
	REQUIRE(db.saveAll());
	CHECK(db.history(0).value().size() == 2);

	watch(db, 1, 3, noteOf("third"));
	auto steps = db.history(0).value();
	REQUIRE(steps.size() == 2);
	CHECK(steps[0].label == "third");
	CHECK(steps[1].label == "first");
	CHECK(db.redo(1, false).value().steps.empty());
	CHECK(progressOf(db, 2) == 6);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue keeps the last 100 steps") {
	catalogue db(tempDir() / "data");
	seedForUndo(db);
	for (int i = 1; i <= catalogue::kHistoryLength + 5; ++i)
		watch(db, 1, i, noteOf(std::to_string(i)));
	auto steps = db.history(0).value();
	REQUIRE(steps.size() == static_cast<size_t>(catalogue::kHistoryLength));
	CHECK(steps.front().label == "105");
	CHECK(steps.back().label == "6");
	CHECK(steps.back().changes == 1);
	auto all = db.undo(1000, false);
	REQUIRE(all.has_value());
	CHECK(all.value().steps.size() == static_cast<size_t>(catalogue::kHistoryLength));
	CHECK(progressOf(db, 1) == 5);
}

TEST_CASE("catalogue data dir: CATALIST_DATA_DIR, then XDG_DATA_HOME, then ~/.local/share") {
	using dirs = catalist::dataDirEnv;
	auto dataDir = [](const dirs& env) { return catalist::dataDirFor(env); };
	CHECK(dataDir(dirs{"/custom", "/xdg/data", "/home/me"}) == "/custom");
	CHECK(dataDir(dirs{"", "/xdg/data", "/home/me"}) == "/xdg/data/catalist");
	CHECK(dataDir(dirs{"", "", "/home/me"}) == "/home/me/.local/share/catalist");
	// A relative XDG_DATA_HOME is not valid, and is ignored.
	CHECK(dataDir(dirs{"", "relative/data", "/home/me"}) == "/home/me/.local/share/catalist");
	CHECK(dataDir(dirs{}) == "catalist-data");

	catalogue db("/data");
	CHECK(db.getDataDir() == "/data");
	CHECK(db.getStorePath() == "/data/catalist.db");
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "catalogue keeps Kitsu ids beside the entries Catalist-TUI writes") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Sousou no Frieren").malId(52991).kitsuId(46474));
	db.addEntry(makeEntry().title("Odd Taxi").malId(44265));
	REQUIRE(db.saveAll());
	{
		catalogue again(tempDir() / "data");
		REQUIRE(again.loadAll().value());
		CHECK(again.getById(mediaType::kAnime, 1)->kitsuId == 46474);
		CHECK(again.getById(mediaType::kAnime, 2)->kitsuId == 0);
		CHECK(again.findByIds(mediaType::kAnime, 0, 0, 46474)->title == "Sousou no Frieren");
	}
	// The file stays one Catalist-TUI opens and writes: the same version, and
	// `entry` with the columns it binds.
	catalist::store::database raw;
	REQUIRE(raw.open(db.getStorePath()));
	auto version = raw.prepare("PRAGMA user_version");
	REQUIRE(version.step());
	CHECK(version.intAt(0) == 1);
	auto columns = raw.prepare("SELECT COUNT(*) FROM pragma_table_info('entry')");
	REQUIRE(columns.step());
	CHECK(columns.intAt(0) == 17);
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "catalogue drops a Kitsu id whose title another program replaced") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Sousou no Frieren").malId(52991).kitsuId(46474));
	REQUIRE(db.saveAll());
	{
		// Catalist-TUI removes the title, and gives its ref to the next one.
		catalist::store::database raw;
		REQUIRE(raw.open(db.getStorePath()));
		REQUIRE(raw.run("UPDATE entry SET title = 'Dandadan', mal_id = 57334 WHERE id = 1"));
	}
	catalogue again(tempDir() / "data");
	REQUIRE(again.loadAll().value());
	CHECK(again.getById(mediaType::kAnime, 1)->kitsuId == 0);
	// Saved again, the stale row is gone.
	again.addEntry(makeEntry().title("Odd Taxi").malId(44265));
	REQUIRE(again.saveAll());
	catalist::store::database raw;
	REQUIRE(raw.open(db.getStorePath()));
	auto rows = raw.prepare("SELECT COUNT(*) FROM entry_id");
	REQUIRE(rows.step());
	CHECK(rows.intAt(0) == 0);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue reads a file that has no Kitsu ids yet") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Odd Taxi").malId(44265));
	REQUIRE(db.saveAll());
	{
		catalist::store::database raw;
		REQUIRE(raw.open(db.getStorePath()));
		REQUIRE(raw.run("DROP TABLE entry_id"));
	}
	// The completion scripts' read-only look, too.
	catalogue peeked(tempDir() / "data");
	CHECK(peeked.peek());
	catalogue again(tempDir() / "data");
	REQUIRE(again.loadAll().value());
	CHECK(again.getEntries(mediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue undo brings back a removed title's Kitsu id") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Sousou no Frieren").malId(52991).kitsuId(46474));
	REQUIRE(db.saveAll());
	REQUIRE(db.removeEntry(mediaType::kAnime, 1));
	REQUIRE(db.saveAll(noteOf("rm frieren")));
	REQUIRE(db.undo(1, false).has_value());
	catalogue again(tempDir() / "data");
	REQUIRE(again.loadAll().value());
	CHECK(again.getById(mediaType::kAnime, 1)->kitsuId == 46474);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue merges an added title known by its Kitsu id") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Sousou no Frieren").kitsuId(46474));
	int again = db.addEntry(makeEntry().title("Frieren").kitsuId(46474).malId(52991));
	CHECK(again == 1);
	CHECK(db.getEntries(mediaType::kAnime).size() == 1);
	CHECK(db.getById(mediaType::kAnime, 1)->malId == 52991);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue keeps what the sites are to be told, once each") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Odd Taxi"));
	REQUIRE(db.saveAll());
	CHECK(db.outbox().empty());
	using catalist::pending;
	using catalist::provider;
	REQUIRE(db.queue({{provider::kAniList, mediaType::kAnime, 1},
	                  {provider::kKitsu, mediaType::kAnime, 1},
	                  {provider::kAniList, mediaType::kAnime, 1}}));
	REQUIRE(db.queue({{provider::kKitsu, mediaType::kManga, 3, true, 46474}}));
	auto waiting = db.outbox();
	REQUIRE(waiting.size() == 3);
	CHECK(waiting[0].site == provider::kAniList);
	CHECK(waiting[2].removed);
	CHECK(waiting[2].siteId == 46474);
	REQUIRE(db.sent(waiting[0]));
	catalogue again(tempDir() / "data");
	REQUIRE(again.loadAll().value());
	CHECK(again.outbox().size() == 2);
}
