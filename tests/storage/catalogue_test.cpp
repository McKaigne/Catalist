#include "storage/catalogue.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>

#include "storage/csv_codec.hpp"
#include "support/test_support.hpp"
#include "system.hpp"

using catalist::FormatDate;
using catalist::MediaType;
using catalist::WatchStatus;
namespace csv_codec = catalist::csv_codec;

using catalist::Catalogue;
using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::MakeEntry;
using catalist::test::TempDirFixture;

TEST_CASE_FIXTURE(TempDirFixture, "catalogue round trip keeps quotes, commas and dates") {
	Catalogue db(TempDir());
	catalist::Entry original = MakeEntry()
	                               .MalId(9901)
	                               .Title("Fate/stay night: Heavens Feel, Part \"1\"")
	                               .Status(WatchStatus::kCompleted)
	                               .Score(10)
	                               .Progress(3)
	                               .TotalUnits(3)
	                               .StartDate("2017-10-14")
	                               .FinishDate("2020-08-15");
	int id = db.AddEntry(original);
	REQUIRE(db.SaveAll());

	Catalogue reloaded(TempDir());
	REQUIRE(reloaded.LoadAll().value());
	auto loaded = reloaded.GetById(MediaType::kAnime, id);
	REQUIRE(loaded.has_value());
	CHECK(loaded->title == original.title);
	CHECK(loaded->score == 10);
	CHECK(loaded->progress == 3);
	CHECK(loaded->start_date == original.start_date);
	CHECK(loaded->finish_date == original.finish_date);
	CHECK_FALSE(loaded->updated_at.has_value());
}

TEST_CASE_FIXTURE(TempDirFixture,
                  "catalogue gives each new title the next free id per media type") {
	Catalogue db(TempDir() / "data");
	CHECK(db.AddEntry(MakeEntry().Title("One")) == 1);
	CHECK(db.AddEntry(MakeEntry().Title("Two").MalId(5)) == 2);
	// An id the caller sets is not kept: ids are only row numbers.
	CHECK(db.AddEntry(MakeEntry().Id(500).Title("Three")) == 3);
	CHECK(db.AddEntry(MakeEntry().Title("Manga").Type(MediaType::kManga)) == 1);

	REQUIRE(db.RemoveEntry(MediaType::kAnime, 2));
	CHECK(db.AddEntry(MakeEntry().Title("Four")) == 4);
	REQUIRE(db.RemoveEntry(MediaType::kAnime, 4));
	REQUIRE(db.RemoveEntry(MediaType::kAnime, 3));
	CHECK(db.AddEntry(MakeEntry().Title("Five")) == 2);
	CHECK_FALSE(db.RemoveEntry(MediaType::kAnime, 99));
}

TEST_CASE_FIXTURE(TempDirFixture,
                  "catalogue continues numbering after the ids in the stored file") {
	{
		Catalogue first(TempDir() / "data");
		first.AddEntry(MakeEntry().Title("Loaded"));
		REQUIRE(first.SaveAll());
		catalist::store::Database raw;
		REQUIRE(raw.Open(first.StorePath()));
		REQUIRE(raw.Run("UPDATE entry SET id = 40"));
	}
	Catalogue db(TempDir() / "data");
	REQUIRE(db.LoadAll().value());
	CHECK(db.GetById(MediaType::kAnime, 40)->title == "Loaded");
	CHECK(db.AddEntry(MakeEntry().Title("Next")) == 41);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue getById and findByIds look in one media type only") {
	Catalogue db(TempDir() / "data");
	int anime = db.AddEntry(MakeEntry().Title("Anime").MalId(1).AnilistId(11));
	db.AddEntry(MakeEntry().Title("Manga").MalId(1).Type(MediaType::kManga));
	CHECK(db.GetById(MediaType::kAnime, anime)->title == "Anime");
	CHECK(db.FindByIds(MediaType::kManga, 0, 1)->title == "Manga");
	CHECK(db.FindByIds(MediaType::kAnime, 11, 0)->title == "Anime");
	CHECK(db.FindByIds(MediaType::kAnime, 99, 1)->title == "Anime");
	CHECK_FALSE(db.FindByIds(MediaType::kAnime, 0, 2).has_value());
	CHECK_FALSE(db.FindByIds(MediaType::kAnime, 0, 0).has_value());
	CHECK_FALSE(db.GetById(MediaType::kAnime, 99).has_value());
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue merges an added title known by either id") {
	Catalogue db(TempDir() / "data");
	db.AddEntry(MakeEntry().Title("Frieren").MalId(52991));
	int merged = db.AddEntry(MakeEntry().Title("Frieren").MalId(52991).AnilistId(154587));
	CHECK(db.GetEntries(MediaType::kAnime).size() == 1);
	CHECK(db.GetById(MediaType::kAnime, merged)->anilist_id == 154587);
	db.AddEntry(MakeEntry().Title("Frieren").AnilistId(154587));
	CHECK(db.GetEntries(MediaType::kAnime).size() == 1);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue updateEntry replaces the entry with the same id") {
	Catalogue db(TempDir() / "data");
	int id = db.AddEntry(MakeEntry().Title("Before"));
	auto changed = *db.GetById(MediaType::kAnime, id);
	changed.title = "After";
	CHECK(db.UpdateEntry(changed));
	CHECK(db.GetById(MediaType::kAnime, id)->title == "After");
	changed.id = 99;
	CHECK_FALSE(db.UpdateEntry(changed));
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue loadAll says whether anything was stored") {
	Catalogue db(TempDir() / "data");
	auto loaded = db.LoadAll();
	REQUIRE(loaded.has_value());
	CHECK_FALSE(loaded.value());
	// Looking at an empty Catalogue creates nothing.
	CHECK_FALSE(std::filesystem::exists(TempDir() / "data"));

	db.AddEntry(MakeEntry().Title("First"));
	REQUIRE(db.SaveAll());
	CHECK(db.LoadAll().value());
	Catalogue again(TempDir() / "data");
	CHECK(again.LoadAll().value());
}

TEST_CASE_FIXTURE(TempDirFixture,
                  "addEntry re-add keeps user fields and refreshes upstream metadata") {
	Catalogue db(TempDir() / "data");
	int id = db.AddEntry(MakeEntry()
	                         .MalId(52991)
	                         .Title("Sousou no Frieren")
	                         .Score(8)
	                         .Progress(5)
	                         .TotalUnits(0)
	                         .StartDate("2023-09-29"));
	int merged_id = db.AddEntry(MakeEntry()
	                                .MalId(52991)
	                                .Title("Frieren (remote)")
	                                .Score(3)
	                                .Progress(20)
	                                .TotalUnits(28)
	                                .TotalVolumes(4)
	                                .StartDate("1999-01-01"));

	CHECK(merged_id == id);
	REQUIRE(db.GetEntries(MediaType::kAnime).size() == 1);
	auto merged = *db.GetById(MediaType::kAnime, id);
	CHECK(merged.score == 8);
	CHECK(merged.progress == 5);
	CHECK(catalist::FormatDate(merged.start_date) == "2023-09-29");
	CHECK(merged.title == "Sousou no Frieren");
	CHECK(merged.total_units == 28);
	CHECK(merged.total_volumes == 4);
}

TEST_CASE_FIXTURE(TempDirFixture, "addEntry re-add backfills empty user fields") {
	Catalogue db(TempDir() / "data");
	int id = db.AddEntry(MakeEntry().MalId(100).Title("Blank").Status(WatchStatus::kCompleted));
	db.AddEntry(
	    MakeEntry().MalId(100).Title("Blank").Score(7).Progress(3).FinishDate("2024-01-01"));
	auto merged = *db.GetById(MediaType::kAnime, id);
	CHECK(merged.score == 7);
	CHECK(merged.progress == 3);
	CHECK(catalist::FormatDate(merged.finish_date) == "2024-01-01");
}

TEST_CASE_FIXTURE(TempDirFixture, "addEntry matches by MyAnimeList id, never by title") {
	Catalogue db(TempDir() / "data");
	db.AddEntry(MakeEntry().MalId(777).Title("Local"));
	db.AddEntry(MakeEntry().Title("Remote").TotalUnits(12).MalId(777));
	db.AddEntry(MakeEntry().Title("Same title"));
	db.AddEntry(MakeEntry().Title("Same title"));
	db.AddEntry(MakeEntry().Title("Local").MalId(778));

	const auto& anime = db.GetEntries(MediaType::kAnime);
	REQUIRE(anime.size() == 4);
	CHECK(anime[0].title == "Local");
	CHECK(anime[0].total_units == 12);
	CHECK(std::count_if(anime.begin(), anime.end(),
	                    [](const auto& e) { return e.title == "Same title"; }) == 2);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue saves both lists and reloads them") {
	Catalogue db(TempDir() / "data");
	int id = db.AddEntry(MakeEntry()
	                         .Title("Kept, \"quoted\"")
	                         .MalId(7)
	                         .AnilistId(70)
	                         .Status(WatchStatus::kOnHold)
	                         .Score(6)
	                         .Progress(3)
	                         .TotalUnits(12)
	                         .Rewatched(2)
	                         .StartDate("2026-01-02")
	                         .FinishDate("2026-02-03")
	                         .UpdatedAt("2026-03-04"));
	db.AddEntry(MakeEntry().Title("Paper").Type(MediaType::kManga).Volumes(3).TotalVolumes(9));
	REQUIRE(db.SaveAll());
	CHECK(std::filesystem::exists(TempDir() / "data/catalist.db"));
	// One file holds everything; no CSV or settings file is written.
	CHECK_FALSE(std::filesystem::exists(TempDir() / "data/anime.csv"));
	CHECK_FALSE(std::filesystem::exists(TempDir() / "data/settings"));

	Catalogue reloaded(TempDir() / "data");
	REQUIRE(reloaded.LoadAll().value());
	CHECK(csv_codec::EncodeEntry(*reloaded.GetById(MediaType::kAnime, id)) ==
	      "1,7,anime,\"Kept, \"\"quoted\"\"\",On-Hold,6,3,12,0,0,2,2026-01-02,2026-02-03,"
	      "2026-03-04,70,,");
	CHECK(csv_codec::EncodeEntry(reloaded.GetEntries(MediaType::kManga)[0]) ==
	      "1,0,manga,Paper,Reading,0,0,0,3,9,0,,,,0,,");
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue writes only the rows it changed") {
	auto dir = TempDir() / "data";
	{
		Catalogue db(dir);
		db.AddEntry(MakeEntry().Title("One"));
		db.AddEntry(MakeEntry().Title("Two"));
		db.AddEntry(MakeEntry().Title("Three"));
		REQUIRE(db.SaveAll());
	}
	// Two programs have the lists open; each changes another title.
	Catalogue first(dir);
	Catalogue second(dir);
	REQUIRE(first.LoadAll().value());
	REQUIRE(second.LoadAll().value());
	auto one = *first.GetById(MediaType::kAnime, 1);
	one.score = 9;
	REQUIRE(first.UpdateEntry(one));
	REQUIRE(first.SaveAll());
	auto two = *second.GetById(MediaType::kAnime, 2);
	two.progress = 4;
	REQUIRE(second.UpdateEntry(two));
	REQUIRE(second.RemoveEntry(MediaType::kAnime, 3));
	REQUIRE(second.SaveAll());

	Catalogue after(dir);
	REQUIRE(after.LoadAll().value());
	CHECK(after.GetById(MediaType::kAnime, 1)->score == 9);
	CHECK(after.GetById(MediaType::kAnime, 2)->progress == 4);
	CHECK_FALSE(after.GetById(MediaType::kAnime, 3).has_value());
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue beginWrite holds the catalogue until the save") {
	auto dir = TempDir() / "data";
	{
		// A command that gives up before saving leaves nothing behind.
		Catalogue db(dir);
		REQUIRE(db.BeginWrite().has_value());
		REQUIRE(db.LoadAll().has_value());
		db.AddEntry(MakeEntry().Title("Dropped on the way"));
	}
	{
		Catalogue db(dir);
		REQUIRE(db.BeginWrite().has_value());
		REQUIRE(db.LoadAll().has_value());
		CHECK(db.GetEntries(MediaType::kAnime).empty());
		db.AddEntry(MakeEntry().Title("Kept"));
		REQUIRE(db.SaveAll());
		// The save ended the write: another program can write again.
		Catalogue other(dir);
		REQUIRE(other.BeginWrite().has_value());
	}
	Catalogue after(dir);
	REQUIRE(after.LoadAll().value());
	REQUIRE(after.GetEntries(MediaType::kAnime).size() == 1);
	CHECK(after.GetEntries(MediaType::kAnime)[0].title == "Kept");
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue reports a failed write and a file it cannot open") {
	Catalogue db(TempDir() / "data");
	catalist::test::MakeUnwritable(TempDir() / "data/catalist.db");
	db.AddEntry(MakeEntry().Title("Nowhere to go"));
	CHECK_FALSE(db.SaveAll());
	CHECK_FALSE(db.BeginWrite().has_value());
	auto loaded = db.LoadAll();
	REQUIRE_FALSE(loaded.has_value());
	CHECK(loaded.error().find("cannot open " + (TempDir() / "data/catalist.db").string() + ": ") ==
	      0);

	catalist::test::WriteText(TempDir() / "other/catalist.db", "this is not a database");
	Catalogue broken(TempDir() / "other");
	CHECK_FALSE(broken.LoadAll().has_value());
	CHECK(catalist::test::ReadText(TempDir() / "other/catalist.db") == "this is not a database");
}

namespace {

catalist::TxNote NoteOf(const std::string& label) {
	return {label, "2026-10-20 21:30:00"};
}

// Frieren (a:1, completed) and Odd Taxi (a:2, 5 of 13) and Berserk (m:1),
// saved without a step in the history.
void SeedForUndo(Catalogue& db) {
	db.AddEntry(MakeEntry().Title("Sousou no Frieren").MalId(52991).AnilistId(154587).Score(10));
	db.AddEntry(
	    MakeEntry().Title("Odd Taxi").MalId(44265).AnilistId(128547).Progress(5).TotalUnits(13));
	db.AddEntry(MakeEntry().Title("Berserk").Type(MediaType::kManga).MalId(2).Progress(364));
	REQUIRE(db.SaveAll());
}

// Sets the progress of an anime and saves it as one step.
void Watch(Catalogue& db, int id, int progress, const catalist::TxNote& note) {
	auto title = *db.GetById(MediaType::kAnime, id);
	title.progress = progress;
	REQUIRE(db.UpdateEntry(title));
	REQUIRE(db.SaveAll(note));
}

int ProgressOf(const Catalogue& db, int id) {
	return db.GetById(MediaType::kAnime, id)->progress;
}

}  // namespace

TEST_CASE_FIXTURE(TempDirFixture, "catalogue enters a save with a note in the undo history") {
	Catalogue db(TempDir() / "data");
	CHECK(db.History(0).value().empty());
	SeedForUndo(db);
	CHECK(db.History(0).value().empty());

	Watch(db, 2, 6, NoteOf("+ taxi"));
	auto berserk = *db.GetById(MediaType::kManga, 1);
	berserk.score = 10;
	REQUIRE(db.UpdateEntry(berserk));
	Watch(db, 1, 3, NoteOf("set a:1 progress=3"));

	auto steps = db.History(0).value();
	REQUIRE(steps.size() == 2);
	// Newest first.
	CHECK(steps[0].id == 2);
	CHECK(steps[0].label == "set a:1 progress=3");
	CHECK(steps[0].stamp == "2026-10-20 21:30:00");
	CHECK(steps[0].changes == 2);
	CHECK_FALSE(steps[0].undone);
	CHECK(steps[1].label == "+ taxi");
	CHECK(steps[1].changes == 1);
	CHECK(db.History(1).value().size() == 1);
	CHECK(db.History(1).value()[0].id == 2);

	// Nothing the person sets has changed: no step.
	REQUIRE(db.SaveAll(NoteOf("nothing")));
	auto taxi = *db.GetById(MediaType::kAnime, 2);
	taxi.total_volumes = 13;
	REQUIRE(db.UpdateEntry(taxi));
	REQUIRE(db.SaveAll(NoteOf("web")));
	CHECK(db.History(0).value().size() == 2);

	// The history is in the file: another program reads the same.
	Catalogue other(TempDir() / "data");
	REQUIRE(other.LoadAll().value());
	CHECK(other.GetById(MediaType::kAnime, 2)->total_volumes == 13);
	CHECK(other.History(0).value().size() == 2);
}

TEST_CASE_FIXTURE(TempDirFixture,
                  "catalogue undo takes back the newest step, redo makes it again") {
	Catalogue db(TempDir() / "data");
	SeedForUndo(db);
	Watch(db, 2, 6, NoteOf("first"));
	Watch(db, 2, 7, NoteOf("second"));

	auto undone = db.Undo(1, false);
	REQUIRE(undone.has_value());
	REQUIRE(undone.value().steps.size() == 1);
	CHECK(undone.value().steps[0].label == "second");
	REQUIRE(undone.value().changes.size() == 1);
	CHECK(undone.value().changes[0].before->progress == 7);
	CHECK(undone.value().changes[0].after->progress == 6);
	CHECK(ProgressOf(db, 2) == 6);
	auto steps = db.History(0).value();
	CHECK(steps[0].undone);
	CHECK_FALSE(steps[1].undone);
	{
		Catalogue saved(TempDir() / "data");
		REQUIRE(saved.LoadAll().value());
		CHECK(ProgressOf(saved, 2) == 6);
	}

	REQUIRE(db.Undo(1, false).has_value());
	CHECK(ProgressOf(db, 2) == 5);
	auto nothing = db.Undo(1, false);
	REQUIRE(nothing.has_value());
	CHECK(nothing.value().steps.empty());
	CHECK(nothing.value().changes.empty());

	// The step undone last is made again first.
	auto redone = db.Redo(1, false);
	REQUIRE(redone.has_value());
	REQUIRE(redone.value().steps.size() == 1);
	CHECK(redone.value().steps[0].label == "first");
	CHECK(ProgressOf(db, 2) == 6);
	REQUIRE(db.Redo(5, false).has_value());
	CHECK(ProgressOf(db, 2) == 7);
	CHECK(db.Redo(1, false).value().steps.empty());
	steps = db.History(0).value();
	for (const auto& step : steps)
		CHECK_FALSE(step.undone);

	// Several at once, newest first; more than there are is all of them.
	auto both = db.Undo(9, false);
	REQUIRE(both.has_value());
	REQUIRE(both.value().steps.size() == 2);
	CHECK(both.value().steps[0].label == "second");
	CHECK(both.value().steps[1].label == "first");
	REQUIRE(both.value().changes.size() == 1);
	CHECK(both.value().changes[0].before->progress == 7);
	CHECK(both.value().changes[0].after->progress == 5);
	Catalogue saved(TempDir() / "data");
	REQUIRE(saved.LoadAll().value());
	CHECK(ProgressOf(saved, 2) == 5);
	CHECK(saved.Redo(2, false).value().steps.size() == 2);
	CHECK(ProgressOf(saved, 2) == 7);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue undo keeps what the web filled in since") {
	Catalogue db(TempDir() / "data");
	SeedForUndo(db);
	Watch(db, 2, 6, NoteOf("+ taxi"));
	auto taxi = *db.GetById(MediaType::kAnime, 2);
	taxi.total_volumes = 13;
	taxi.score = 9;
	REQUIRE(db.UpdateEntry(taxi));
	REQUIRE(db.SaveAll());

	REQUIRE(db.Undo(1, false).has_value());
	taxi = *db.GetById(MediaType::kAnime, 2);
	CHECK(taxi.progress == 5);
	CHECK(taxi.total_volumes == 13);
	CHECK(taxi.score == 9);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue undo changes nothing when a title has changed since") {
	Catalogue db(TempDir() / "data");
	SeedForUndo(db);
	Watch(db, 2, 6, NoteOf("older"));
	Watch(db, 1, 4, NoteOf("newer"));
	// Changed outside the history, as a web check changes a total.
	auto taxi = *db.GetById(MediaType::kAnime, 2);
	taxi.progress = 9;
	REQUIRE(db.UpdateEntry(taxi));
	REQUIRE(db.SaveAll());

	// The newer step could be taken back, the older one not: neither is.
	auto refused = db.Undo(2, false);
	REQUIRE_FALSE(refused.has_value());
	CHECK(refused.error() ==
	      "cannot undo 'older' (2026-10-20 21:30:00): Odd Taxi (a:2) has changed since: "
	      "progress is now 9, not 6");
	CHECK(ProgressOf(db, 1) == 4);
	CHECK(ProgressOf(db, 2) == 9);
	auto steps = db.History(0).value();
	for (const auto& step : steps)
		CHECK_FALSE(step.undone);
	{
		Catalogue saved(TempDir() / "data");
		REQUIRE(saved.LoadAll().value());
		CHECK(ProgressOf(saved, 1) == 4);
		CHECK(ProgressOf(saved, 2) == 9);
	}

	auto forced = db.Undo(2, true);
	REQUIRE(forced.has_value());
	CHECK(forced.value().steps.size() == 2);
	CHECK(ProgressOf(db, 1) == 0);
	CHECK(ProgressOf(db, 2) == 5);

	// Redo checks the same way.
	taxi = *db.GetById(MediaType::kAnime, 2);
	taxi.progress = 1;
	REQUIRE(db.UpdateEntry(taxi));
	REQUIRE(db.SaveAll());
	auto again = db.Redo(1, false);
	REQUIRE_FALSE(again.has_value());
	CHECK(again.error() ==
	      "cannot redo 'older' (2026-10-20 21:30:00): Odd Taxi (a:2) has changed since: "
	      "progress is now 1, not 5");
	CHECK(ProgressOf(db, 2) == 1);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue undo brings a removed title back under its ref") {
	Catalogue db(TempDir() / "data");
	SeedForUndo(db);
	db.AddEntry(MakeEntry().Title("Mushishi"));
	REQUIRE(db.SaveAll());
	auto taxi = *db.GetById(MediaType::kAnime, 2);
	REQUIRE(db.RemoveEntry(MediaType::kAnime, 2));
	REQUIRE(db.SaveAll(NoteOf("rm taxi")));

	// In its place: the lists are kept in the order of their ids.
	CHECK_FALSE(db.RestoreEntry(*db.GetById(MediaType::kAnime, 3)));
	REQUIRE(db.Undo(1, false).has_value());
	auto anime = db.GetEntries(MediaType::kAnime);
	REQUIRE(anime.size() == 3);
	CHECK(anime[1].id == 2);
	CHECK(csv_codec::EncodeEntry(anime[1]) == csv_codec::EncodeEntry(taxi));
	CHECK(anime[2].title == "Mushishi");
	{
		Catalogue saved(TempDir() / "data");
		REQUIRE(saved.LoadAll().value());
		CHECK(saved.GetById(MediaType::kAnime, 2)->title == "Odd Taxi");
	}
	// Redo removes it again; undo of an addition removes what was added.
	REQUIRE(db.Redo(1, false).has_value());
	CHECK_FALSE(db.GetById(MediaType::kAnime, 2));
	int added = db.AddEntry(MakeEntry().Title("Dandadan").AnilistId(171018));
	REQUIRE(db.SaveAll(NoteOf("add dandadan")));
	REQUIRE(db.Undo(1, false).has_value());
	CHECK_FALSE(db.GetById(MediaType::kAnime, added));
	REQUIRE(db.Redo(1, false).has_value());
	CHECK(db.GetById(MediaType::kAnime, added)->title == "Dandadan");
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue undo cannot bring a title back to a ref in use") {
	Catalogue db(TempDir() / "data");
	SeedForUndo(db);
	REQUIRE(db.RemoveEntry(MediaType::kAnime, 2));
	REQUIRE(db.SaveAll(NoteOf("rm taxi")));
	// The highest id was freed, and the next title added got it.
	CHECK(db.AddEntry(MakeEntry().Title("Mushishi")) == 2);
	REQUIRE(db.SaveAll());

	for (bool force : {false, true}) {
		auto refused = db.Undo(1, force);
		REQUIRE_FALSE(refused.has_value());
		CHECK(refused.error() ==
		      "cannot undo 'rm taxi' (2026-10-20 21:30:00): Odd Taxi (a:2) cannot come back: "
		      "its ref is now that of Mushishi");
	}
	CHECK(db.GetById(MediaType::kAnime, 2)->title == "Mushishi");
	CHECK_FALSE(db.History(0).value()[0].undone);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue forgets what was undone when new work is saved") {
	Catalogue db(TempDir() / "data");
	SeedForUndo(db);
	Watch(db, 2, 6, NoteOf("first"));
	Watch(db, 2, 7, NoteOf("second"));
	REQUIRE(db.Undo(1, false).has_value());
	// A save outside the history leaves what can be redone.
	auto frieren = *db.GetById(MediaType::kAnime, 1);
	frieren.total_volumes = 28;
	REQUIRE(db.UpdateEntry(frieren));
	REQUIRE(db.SaveAll());
	CHECK(db.History(0).value().size() == 2);

	Watch(db, 1, 3, NoteOf("third"));
	auto steps = db.History(0).value();
	REQUIRE(steps.size() == 2);
	CHECK(steps[0].label == "third");
	CHECK(steps[1].label == "first");
	CHECK(db.Redo(1, false).value().steps.empty());
	CHECK(ProgressOf(db, 2) == 6);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue keeps the last 100 steps") {
	Catalogue db(TempDir() / "data");
	SeedForUndo(db);
	for (int i = 1; i <= Catalogue::kHistoryLength + 5; ++i)
		Watch(db, 1, i, NoteOf(std::to_string(i)));
	auto steps = db.History(0).value();
	REQUIRE(steps.size() == static_cast<size_t>(Catalogue::kHistoryLength));
	CHECK(steps.front().label == "105");
	CHECK(steps.back().label == "6");
	CHECK(steps.back().changes == 1);
	auto all = db.Undo(1000, false);
	REQUIRE(all.has_value());
	CHECK(all.value().steps.size() == static_cast<size_t>(Catalogue::kHistoryLength));
	CHECK(ProgressOf(db, 1) == 5);
}

TEST_CASE("catalogue data dir: CATALIST_DATA_DIR, then XDG_DATA_HOME, then ~/.local/share") {
	using Dirs = catalist::DataDirEnv;
	auto data_dir = [](const Dirs& env) { return catalist::DataDirFor(env); };
	CHECK(data_dir(Dirs{"/custom", "/xdg/data", "/home/me"}) == "/custom");
	CHECK(data_dir(Dirs{"", "/xdg/data", "/home/me"}) == "/xdg/data/catalist");
	CHECK(data_dir(Dirs{"", "", "/home/me"}) == "/home/me/.local/share/catalist");
	// A relative XDG_DATA_HOME is not valid, and is ignored.
	CHECK(data_dir(Dirs{"", "relative/data", "/home/me"}) == "/home/me/.local/share/catalist");
	CHECK(data_dir(Dirs{}) == "catalist-data");

	Catalogue db("/data");
	CHECK(db.data_dir() == "/data");
	CHECK(db.StorePath() == "/data/catalist.db");
}

TEST_CASE_FIXTURE(TempDirFixture,
                  "catalogue brings a version 1 file up to date, keeping its lists") {
	std::filesystem::create_directories(TempDir() / "data");
	{
		catalist::store::Database raw;
		REQUIRE(raw.Open(TempDir() / "data/catalist.db"));
		REQUIRE(raw.Run(
		    "CREATE TABLE entry (type TEXT NOT NULL, id INTEGER NOT NULL, mal_id INTEGER NOT NULL,"
		    " title TEXT NOT NULL, status TEXT NOT NULL, score INTEGER NOT NULL,"
		    " progress INTEGER NOT NULL, total_units INTEGER NOT NULL, volumes INTEGER NOT NULL,"
		    " total_volumes INTEGER NOT NULL, rewatched INTEGER NOT NULL, start_date TEXT,"
		    " finish_date TEXT, updated_at TEXT, released INTEGER NOT NULL, checked_at TEXT,"
		    " anilist_id INTEGER NOT NULL, PRIMARY KEY (type, id));"
		    "CREATE TABLE setting (key TEXT PRIMARY KEY, value TEXT NOT NULL);"
		    "CREATE TABLE tx (id INTEGER PRIMARY KEY AUTOINCREMENT, stamp TEXT NOT NULL,"
		    " label TEXT NOT NULL, undone INTEGER NOT NULL DEFAULT 0);"
		    "CREATE TABLE change (tx INTEGER NOT NULL, seq INTEGER NOT NULL, type TEXT NOT NULL,"
		    " id INTEGER NOT NULL, before TEXT NOT NULL, after TEXT NOT NULL,"
		    " PRIMARY KEY (tx, seq));"
		    "CREATE TABLE entry_id (type TEXT, id INTEGER, site TEXT, site_id INTEGER);"
		    "CREATE TABLE outbox (seq INTEGER PRIMARY KEY);"
		    "CREATE TABLE track_option (site TEXT);"
		    "INSERT INTO entry VALUES ('anime', 3, 44265, 'Odd Taxi', 'watching', 8, 5, 13, 0, 0,"
		    " 1, '2026-10-01', NULL, '2026-10-02', 13, '2026-10-03', 128547);"
		    "INSERT INTO setting VALUES ('provider', 'kitsu');"
		    "INSERT INTO tx (stamp, label) VALUES ('2026-10-02 10:00:00', 'import');"
		    "INSERT INTO change VALUES (1, 0, 'anime', 3, '', 'old row');"
		    "PRAGMA user_version = 1;"));
	}
	Catalogue db(TempDir() / "data");
	REQUIRE(db.LoadAll().value());
	auto taxi = db.GetById(MediaType::kAnime, 3);
	REQUIRE(taxi.has_value());
	CHECK(taxi->title == "Odd Taxi");
	CHECK(taxi->mal_id == 44265);
	CHECK(taxi->anilist_id == 128547);
	CHECK(taxi->score == 8);
	CHECK(taxi->progress == 5);
	CHECK(taxi->total_units == 13);
	CHECK(taxi->rewatched == 1);
	CHECK(FormatDate(taxi->start_date) == "2026-10-01");
	CHECK(FormatDate(taxi->updated_at) == "2026-10-02");
	// The old undo rows have columns that are gone.
	CHECK(db.History(0).value().empty());

	catalist::store::Database raw;
	REQUIRE(raw.Open(TempDir() / "data/catalist.db"));
	auto version = raw.Prepare("PRAGMA user_version");
	REQUIRE(version.Step());
	CHECK(version.IntAt(0) == 3);
	auto columns = raw.Prepare("SELECT COUNT(*) FROM pragma_table_info('entry')");
	REQUIRE(columns.Step());
	CHECK(columns.IntAt(0) == 17);
	auto gone = raw.Prepare(
	    "SELECT COUNT(*) FROM sqlite_master WHERE name IN"
	    " ('setting', 'entry_id', 'outbox', 'track_option')");
	REQUIRE(gone.Step());
	CHECK(gone.IntAt(0) == 0);
}

namespace {

void WriteVersionTwo(const std::filesystem::path& file) {
	catalist::store::Database raw;
	REQUIRE(raw.Open(file));
	REQUIRE(raw.Run(
	    "CREATE TABLE entry (type TEXT NOT NULL, id INTEGER NOT NULL, mal_id INTEGER NOT NULL,"
	    " title TEXT NOT NULL, status TEXT NOT NULL, score INTEGER NOT NULL,"
	    " progress INTEGER NOT NULL, total_units INTEGER NOT NULL, volumes INTEGER NOT NULL,"
	    " total_volumes INTEGER NOT NULL, rewatched INTEGER NOT NULL, start_date TEXT,"
	    " finish_date TEXT, updated_at TEXT, anilist_id INTEGER NOT NULL,"
	    " PRIMARY KEY (type, id));"
	    "CREATE TABLE tx (id INTEGER PRIMARY KEY AUTOINCREMENT, stamp TEXT NOT NULL,"
	    " label TEXT NOT NULL, undone INTEGER NOT NULL DEFAULT 0);"
	    "CREATE TABLE change (tx INTEGER NOT NULL, seq INTEGER NOT NULL, type TEXT NOT NULL,"
	    " id INTEGER NOT NULL, before TEXT NOT NULL, after TEXT NOT NULL,"
	    " PRIMARY KEY (tx, seq));"
	    "INSERT INTO entry VALUES ('anime', 1, 1, 'Paused', 'on-hold', 0, 3, 12, 0, 0, 0,"
	    " '2026-01-01', '2026-02-01', '2026-02-01', 0);"
	    "INSERT INTO entry VALUES ('anime', 2, 2, 'Finished', 'completed', 0, 12, 12, 0, 0, 0,"
	    " '2026-01-01', '2026-02-02', '2026-02-02', 0);"
	    "INSERT INTO entry VALUES ('anime', 3, 3, 'Again', 'rewatching', 0, 1, 12, 0, 0, 0,"
	    " '2026-01-01', '2026-02-03', '2026-02-03', 0);"
	    "INSERT INTO tx (stamp, label) VALUES ('2026-02-03 10:00:00', 'set');"
	    "PRAGMA user_version = 2;"));
}

std::vector<std::filesystem::path> BackupsIn(const std::filesystem::path& dir) {
	std::vector<std::filesystem::path> found;
	for (const auto& each : std::filesystem::directory_iterator(dir))
		if (each.path().filename().string().starts_with("catalist-v"))
			found.push_back(each.path());
	return found;
}

}  // namespace

TEST_CASE_FIXTURE(TempDirFixture,
                  "catalogue upgrades version 2, backing it up and clearing stray finish dates") {
	auto dir = TempDir() / "data";
	std::filesystem::create_directories(dir);
	WriteVersionTwo(dir / "catalist.db");

	Catalogue db(dir);
	REQUIRE(db.LoadAll().value());
	CHECK_FALSE(db.GetById(MediaType::kAnime, 1)->finish_date.has_value());
	CHECK(FormatDate(db.GetById(MediaType::kAnime, 2)->finish_date) == "2026-02-02");
	CHECK(FormatDate(db.GetById(MediaType::kAnime, 3)->finish_date) == "2026-02-03");
	CHECK(db.History(0).value().size() == 1);

	auto backups = BackupsIn(dir);
	REQUIRE(backups.size() == 1);
	CHECK(backups[0].filename().string().starts_with("catalist-v2-"));
	catalist::store::Database copy;
	REQUIRE(copy.Open(backups[0]));
	auto version = copy.Prepare("PRAGMA user_version");
	REQUIRE(version.Step());
	CHECK(version.IntAt(0) == 2);
	auto kept = copy.Prepare("SELECT finish_date FROM entry WHERE id = 1");
	REQUIRE(kept.Step());
	CHECK(kept.TextAt(0) == "2026-02-01");

	Catalogue again(dir);
	REQUIRE(again.LoadAll().value());
	CHECK(BackupsIn(dir).size() == 1);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue leaves a store alone when its backup fails") {
	auto dir = TempDir() / "data";
	std::filesystem::create_directories(dir);
	WriteVersionTwo(dir / "catalist.db");
	std::filesystem::permissions(dir, std::filesystem::perms::owner_write,
	                             std::filesystem::perm_options::remove);
	{
		Catalogue db(dir);
		auto loaded = db.LoadAll();
		REQUIRE_FALSE(loaded.has_value());
		CHECK(loaded.error().find("cannot back up") != std::string::npos);
	}
	std::filesystem::permissions(dir, std::filesystem::perms::owner_write,
	                             std::filesystem::perm_options::add);
	catalist::store::Database raw;
	REQUIRE(raw.Open(dir / "catalist.db"));
	auto version = raw.Prepare("PRAGMA user_version");
	REQUIRE(version.Step());
	CHECK(version.IntAt(0) == 2);
}

TEST_CASE_FIXTURE(TempDirFixture,
                  "catalogue stamps the exact time of a change, and undo restores it") {
	Catalogue db(TempDir() / "data");
	int id = db.AddEntry(MakeEntry().Title("Taxi").EnglishTitle("Odd Taxi"));
	REQUIRE(db.SaveAll(catalist::TxNote{"add", "2026-11-23 10:00:00"}));
	CHECK(db.GetById(MediaType::kAnime, id)->changed_at == "2026-11-23 10:00:00");

	auto next = *db.GetById(MediaType::kAnime, id);
	next.score = 7;
	db.UpdateEntry(next);
	REQUIRE(db.SaveAll(catalist::TxNote{"set", "2026-11-23 11:30:00"}));
	CHECK(db.GetById(MediaType::kAnime, id)->changed_at == "2026-11-23 11:30:00");

	REQUIRE(db.Undo(1, false).has_value());
	CHECK(db.GetById(MediaType::kAnime, id)->changed_at == "2026-11-23 10:00:00");

	Catalogue reloaded(TempDir() / "data");
	REQUIRE(reloaded.LoadAll().value());
	CHECK(DisplayName(*reloaded.GetById(MediaType::kAnime, id)) == "Odd Taxi");
	CHECK(reloaded.GetById(MediaType::kAnime, id)->title == "Taxi");
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue refuses a file from a newer version") {
	std::filesystem::create_directories(TempDir() / "data");
	{
		catalist::store::Database raw;
		REQUIRE(raw.Open(TempDir() / "data/catalist.db"));
		REQUIRE(raw.Run("PRAGMA user_version = 4;"));
	}
	Catalogue db(TempDir() / "data");
	auto loaded = db.LoadAll();
	REQUIRE_FALSE(loaded.has_value());
	CHECK(loaded.error().find("was written by a newer Catalist") != std::string::npos);
}
