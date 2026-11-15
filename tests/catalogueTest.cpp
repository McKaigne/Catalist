#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>

#include "catalogue.hpp"
#include "support/sampleLibrary.hpp"
#include "support/testSupport.hpp"
#include "system.hpp"

using catalist::formatDate;
using catalist::mediaType;
using catalist::watchStatus;
namespace csvCodec = catalist::csvCodec;

TEST_CASE("csvCodec reads the zero date sentinel as absent and writes it empty") {
	auto decoded =
	    csvCodec::decodeEntries("1,1,anime,Old,Completed,7,1,1,0,0,0,0000-00-00,,0000-00-00\n",
	                            mediaType::kAnime)
	        .value();
	REQUIRE(decoded.size() == 1);
	CHECK_FALSE(decoded[0].startDate.has_value());
	CHECK_FALSE(decoded[0].finishDate.has_value());
	CHECK_FALSE(decoded[0].updatedAt.has_value());

	auto encoded = csvCodec::encodeEntries(decoded);
	CHECK(encoded.find("0000-00-00") == std::string::npos);
	CHECK(encoded.find("\n1,1,anime,Old,Completed,7,1,1,0,0,0,,,,0,,0\n") != std::string::npos);
}

TEST_CASE("csvCodec escapeField quotes only when needed") {
	CHECK(csvCodec::escapeField("plain") == "plain");
	CHECK(csvCodec::escapeField("a,b") == "\"a,b\"");
	CHECK(csvCodec::escapeField("say \"hi\"") == "\"say \"\"hi\"\"\"");
	CHECK(csvCodec::escapeField("two\nlines") == "\"two\nlines\"");
}

TEST_CASE("csvCodec splitRecords joins quoted newlines and skips comments") {
	auto records =
	    csvCodec::splitRecords("# header\r\n\r\na,\"multi\r\nline\",b\r\n# between\nc,d\n");
	REQUIRE(records.size() == 2);
	CHECK(records[0] == "a,\"multi\nline\",b");
	CHECK(records[1] == "c,d");
}

TEST_CASE("csvCodec splitFields handles escaped quotes and empty fields") {
	auto fields = csvCodec::splitFields("1,,\"x \"\"y\"\", z\",");
	REQUIRE(fields.size() == 4);
	CHECK(fields[1] == "");
	CHECK(fields[2] == "x \"y\", z");
	CHECK(fields[3] == "");
}

TEST_CASE("csvCodec decodeEntry rejects short rows and non numeric ids") {
	CHECK_FALSE(csvCodec::decodeEntry({"1", "2", "3"}, mediaType::kAnime));
	CHECK_FALSE(
	    csvCodec::decodeEntry({"x", "0", "T", "Watching", "0", "0", "0"}, mediaType::kAnime));
	// The type column in the right place, but too few columns after it.
	CHECK_FALSE(csvCodec::decodeEntry({"1", "0", "anime", "T", "Watching"}, mediaType::kAnime));
	CHECK_FALSE(csvCodec::decodeEntry(csvCodec::splitFields("x,0,anime,T,Watching,0,0,0,0,0,0,,,"),
	                                  mediaType::kAnime));
}

TEST_CASE("csvCodec writes the 17 columns and reads them back") {
	catalist::entry original = catalist::test::makeEntry()
	                               .id(5)
	                               .malId(6)
	                               .anilistId(306)
	                               .title("R, \"t\"")
	                               .type(mediaType::kManga)
	                               .status(watchStatus::kOnHold)
	                               .score(4)
	                               .progress(3)
	                               .totalUnits(9)
	                               .volumes(1)
	                               .totalVolumes(2)
	                               .rewatched(1)
	                               .startDate("2024-01-02")
	                               .updatedAt("2024-02-03")
	                               .released(7)
	                               .checkedAt("2024-02-05");
	auto text = csvCodec::encodeEntries({original});
	CHECK(text ==
	      "#id,mal_id,type,title,status,score,progress,total_units,volumes,"
	      "total_volumes,rewatched,start_date,finish_date,updated_at,released,"
	      "checked_at,anilist_id\n"
	      "5,6,manga,\"R, \"\"t\"\"\",On-Hold,4,3,9,1,2,1,2024-01-02,,"
	      "2024-02-03,7,2024-02-05,306\n");
	auto decoded = csvCodec::decodeEntries(text, mediaType::kManga).value();
	REQUIRE(decoded.size() == 1);
	const auto& copy = decoded[0];
	CHECK(copy.id == 5);
	CHECK(copy.malId == 6);
	CHECK(copy.anilistId == 306);
	CHECK(copy.title == original.title);
	CHECK(copy.type == mediaType::kManga);
	CHECK(copy.status == watchStatus::kOnHold);
	CHECK(copy.score == 4);
	CHECK(copy.progress == 3);
	CHECK(copy.totalUnits == 9);
	CHECK(copy.volumes == 1);
	CHECK(copy.totalVolumes == 2);
	CHECK(copy.rewatched == 1);
	CHECK(formatDate(copy.startDate) == "2024-01-02");
	CHECK_FALSE(copy.finishDate.has_value());
	CHECK(formatDate(copy.updatedAt) == "2024-02-03");
	CHECK(copy.released == 7);
	CHECK(formatDate(copy.checkedAt) == "2024-02-05");
}

TEST_CASE("csvCodec reads rows from before the web check columns") {
	auto decoded = csvCodec::decodeEntries("1,2,manga,Older,Reading,0,5,0,0,0,0,,,2026-01-02\n",
	                                       mediaType::kManga)
	                   .value();
	REQUIRE(decoded.size() == 1);
	CHECK(decoded[0].title == "Older");
	CHECK(decoded[0].progress == 5);
	CHECK(formatDate(decoded[0].updatedAt) == "2026-01-02");
	CHECK(decoded[0].released == 0);
	CHECK_FALSE(decoded[0].checkedAt.has_value());
}

TEST_CASE("csvCodec reads the 0.3 layout and its extra columns, dropping them") {
	auto decoded = csvCodec::decodeEntries(
	                   "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,"
	                   "volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,"
	                   "dirty,external_ids,queue_pos,synced\n"
	                   "5,5,105,anime,Kept,Watching,7,3,12,0,0,2,2024-01-02,,\"a, \"\"b\"\"\","
	                   "2024-02-03,1,kitsu=9;mal=5,2,anilist;mal\n"
	                   "6,0,0,anime,Short,Dropped,0,1,0,0,0,0,,2024-03-04,,\n",
	                   mediaType::kAnime)
	                   .value();
	REQUIRE(decoded.size() == 2);
	CHECK(decoded[0].title == "Kept");
	CHECK(decoded[0].anilistId == 105);
	CHECK(decoded[0].score == 7);
	CHECK(decoded[0].rewatched == 2);
	CHECK(formatDate(decoded[0].startDate) == "2024-01-02");
	CHECK(formatDate(decoded[0].updatedAt) == "2024-02-03");
	CHECK(decoded[1].status == watchStatus::kDropped);
	CHECK(formatDate(decoded[1].finishDate) == "2024-03-04");
	CHECK(csvCodec::encodeEntries(decoded) ==
	      "#id,mal_id,type,title,status,score,progress,total_units,volumes,"
	      "total_volumes,rewatched,start_date,finish_date,updated_at,released,"
	      "checked_at,anilist_id\n"
	      "5,5,anime,Kept,Watching,7,3,12,0,0,2,2024-01-02,,2024-02-03,0,,105\n"
	      "6,0,anime,Short,Dropped,0,1,0,0,0,0,,2024-03-04,,0,,0\n");
}

TEST_CASE("csvCodec reads an unknown status as planned instead of dropping the row") {
	auto decoded = csvCodec::decodeEntries("1,0,manga,Edited by hand,Someday,0,0,0,0,0,0,,,\n",
	                                       mediaType::kManga);
	REQUIRE(decoded.ok());
	REQUIRE(decoded.value().size() == 1);
	CHECK(decoded.value()[0].status == watchStatus::kPlanToWatch);
	CHECK(decoded.value()[0].type == mediaType::kManga);
}

TEST_CASE("csvCodec names the first row it cannot read") {
	auto decoded = csvCodec::decodeEntries(
	    "1,0,anime,Fine,Watching,0,0,0,0,0,0,,,\n"
	    "x,0,anime,Bad id,Watching,0,0,0,0,0,0,,,\n",
	    mediaType::kAnime);
	REQUIRE_FALSE(decoded.ok());
	CHECK(decoded.errorMessage() == "row 2 has a column layout this version cannot read");
}

using catalist::catalogue;
using catalist::mediaType;
using catalist::sortKey;
using catalist::watchStatus;
using catalist::test::makeEntry;
using catalist::test::tempDirFixture;

namespace {

std::vector<std::string> titlesSortedBy(const catalogue& db, sortKey key, bool reverse) {
	auto entries = db.getEntries(mediaType::kAnime);
	catalogue::sortEntries(entries, key, reverse);
	std::vector<std::string> titles;
	for (const auto& e : entries)
		titles.push_back(e.title);
	return titles;
}

using titleList = std::vector<std::string>;

void loadFixture(catalogue& db, mediaType type, const std::string& name) {
	REQUIRE(db.loadCsv(type, catalist::test::fixturePath("csv/" + name)).value());
}

}  // namespace

TEST_CASE_FIXTURE(tempDirFixture, "csv round trip keeps quotes, commas and dates") {
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
                  "catalogue continues numbering after the ids in the loaded file") {
	REQUIRE(catalist::writeFileAtomic(tempDir() / "data/anime.csv",
	                                  "40,0,0,anime,Loaded,Watching,0,0,0,0,0,0,,,,\n"));
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

TEST_CASE_FIXTURE(tempDirFixture, "catalogue finds a title by the provider's id") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Frieren").malId(52991).anilistId(154587));
	db.addEntry(makeEntry().title("Berserk").malId(2).anilistId(30002).type(mediaType::kManga));
	CHECK(db.findOn(catalist::provider::kAniList, 30002)->title == "Berserk");
	CHECK(db.findOn(catalist::provider::kMyAnimeList, 52991)->title == "Frieren");
	CHECK_FALSE(db.findOn(catalist::provider::kAniList, 52991).has_value());
	CHECK_FALSE(db.findOn(catalist::provider::kAniList, 0).has_value());
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue keeps the provider in its settings file") {
	catalogue db(tempDir() / "data");
	REQUIRE(db.loadAll().ok());
	CHECK(db.getProvider() == catalist::provider::kAniList);
	db.setProvider(catalist::provider::kMyAnimeList);
	REQUIRE(db.saveSettings());
	CHECK(catalist::test::readText(tempDir() / "data" / "settings") == "provider = myanimelist\n");

	catalogue reread(tempDir() / "data");
	REQUIRE(reread.loadAll().ok());
	CHECK(reread.getProvider() == catalist::provider::kMyAnimeList);

	// Unknown keys and values are ignored.
	catalist::test::writeText(tempDir() / "data" / "settings",
	                          "theme = dark\nprovider = kitsu\n  provider =  AniList \n");
	catalogue tolerant(tempDir() / "data");
	REQUIRE(tolerant.loadAll().ok());
	CHECK(tolerant.getProvider() == catalist::provider::kAniList);
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

TEST_CASE_FIXTURE(tempDirFixture,
                  "sortEntries puts each key in its natural order and reverse flips it") {
	catalogue db(tempDir() / "data");
	catalist::test::seedSampleLibrary(db);
	const titleList byRating = {"Sousou no Frieren", "Odd Taxi", "Ghost, \"in\" the Shell",
	                            "A Very Long Title That Will Definitely Be "
	                            "Truncated"};
	CHECK(titlesSortedBy(db, sortKey::kScore, false) == byRating);
	CHECK(titlesSortedBy(db, sortKey::kScore, true) ==
	      titleList(byRating.rbegin(), byRating.rend()));
	CHECK(titlesSortedBy(db, sortKey::kProgress, false) == byRating);

	CHECK(titlesSortedBy(db, sortKey::kTitle, false) ==
	      titleList{"A Very Long Title That Will Definitely Be Truncated",
	                "Ghost, \"in\" the Shell", "Odd Taxi", "Sousou no Frieren"});
	CHECK(titlesSortedBy(db, sortKey::kStatus, false) ==
	      titleList{"Odd Taxi", "Sousou no Frieren", "Ghost, \"in\" the Shell",
	                "A Very Long Title That Will Definitely Be Truncated"});
	// By the date the list shows: finish when completed, else start, else the
	// last change.
	CHECK(titlesSortedBy(db, sortKey::kDate, false) ==
	      titleList{"A Very Long Title That Will Definitely Be Truncated", "Sousou no Frieren",
	                "Odd Taxi", "Ghost, \"in\" the Shell"});
}

TEST_CASE_FIXTURE(tempDirFixture, "sortEntries breaks ties by title") {
	catalogue db(tempDir() / "data");
	db.addEntry(makeEntry().title("Charlie").score(7));
	db.addEntry(makeEntry().title("Alpha").score(7));
	db.addEntry(makeEntry().title("Bravo").score(9));
	CHECK(titlesSortedBy(db, sortKey::kScore, false) == titleList{"Bravo", "Alpha", "Charlie"});
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue statistics count, average and estimate time") {
	catalogue db(tempDir() / "data");
	catalist::test::seedSampleLibrary(db);
	CHECK(db.countByStatus(mediaType::kAnime, watchStatus::kWatching) == 1);
	CHECK(db.countByStatus(mediaType::kAnime, watchStatus::kOnHold) == 0);
	CHECK(db.countByStatus(mediaType::kManga, watchStatus::kOnHold) == 1);
	CHECK(db.countByScore(mediaType::kAnime, 10) == 1);
	CHECK(db.countByScore(mediaType::kAnime, 0) == 1);
	// Unrated titles do not pull the mean down.
	CHECK(db.averageScore(mediaType::kAnime) == doctest::Approx(8.0));
	CHECK(db.averageScore(mediaType::kManga) == doctest::Approx(9.5));
	CHECK(db.totalProgress(mediaType::kAnime) == 36);
	CHECK(db.totalProgress(mediaType::kManga) == 461);
	CHECK(db.totalVolumes(mediaType::kManga) == 53);
	CHECK(db.totalRewatched(mediaType::kAnime) == 1);
	CHECK(db.daysSpent(mediaType::kAnime) == doctest::Approx(36 * 24 / 1440.0));
	CHECK(db.daysSpent(mediaType::kManga) == doctest::Approx(461 * 5 / 1440.0));

	catalogue empty(tempDir() / "elsewhere");
	CHECK(empty.averageScore(mediaType::kAnime) == 0.0);
	CHECK(empty.daysSpent(mediaType::kAnime) == 0.0);
}

TEST_CASE_FIXTURE(tempDirFixture,
                  "catalogue refuses a catalogue in an old layout and names the file") {
	for (const char* oldRow : {"52991,52991,Sousou no Frieren,Completed,10,28,28\n",
	                           "52991,52991,154587,Sousou no Frieren,Completed,10,28,28,0,0,1,"
	                           "2023-09-29,2024-03-22,Masterpiece.,2026-10-17,1\n"}) {
		REQUIRE(catalist::writeFileAtomic(tempDir() / "data/anime.csv", oldRow));
		catalogue db(tempDir() / "data");
		auto loaded = db.loadAll();
		REQUIRE_FALSE(loaded.ok());
		CHECK(loaded.errorMessage().find((tempDir() / "data/anime.csv").string() +
		                                 ": row 1 has a column layout") == 0);
		CHECK(db.getEntries(mediaType::kAnime).empty());
	}
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue loadAll says whether there was a catalogue") {
	catalogue db(tempDir() / "data");
	auto loaded = db.loadAll();
	REQUIRE(loaded.ok());
	CHECK_FALSE(loaded.value());
	REQUIRE(catalist::writeFileAtomic(tempDir() / "data/manga.csv", ""));
	CHECK(db.loadAll().value());
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads the current layout") {
	catalogue db(tempDir());
	loadFixture(db, mediaType::kManga, "current.csv");
	const auto& entries = db.getEntries(mediaType::kManga);
	REQUIRE(entries.size() == 1);
	const auto& loaded = entries[0];
	CHECK(loaded.type == mediaType::kManga);
	CHECK(loaded.title == "Berserk");
	CHECK(loaded.status == watchStatus::kWatching);
	CHECK(loaded.progress == 364);
	CHECK(loaded.volumes == 42);
	CHECK(loaded.rewatched == 1);
	CHECK(catalist::formatDate(loaded.startDate) == "2019-05-01");
	CHECK(catalist::formatDate(loaded.finishDate) == "");
	CHECK(catalist::formatDate(loaded.updatedAt) == "2026-10-17");
	CHECK(loaded.malId == 2);
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads a file written by Catalist 0.3") {
	catalogue db(tempDir());
	loadFixture(db, mediaType::kManga, "v03.csv");
	const auto& entries = db.getEntries(mediaType::kManga);
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].id == 2);
	CHECK(entries[0].malId == 2);
	CHECK(entries[0].title == "Berserk");
	CHECK(entries[0].progress == 364);
	CHECK(entries[0].volumes == 42);
	CHECK(catalist::formatDate(entries[0].startDate) == "2019-05-01");
	CHECK(catalist::formatDate(entries[0].updatedAt) == "2026-10-17");
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads crlf line endings") {
	catalogue db(tempDir());
	loadFixture(db, mediaType::kAnime, "crlf.csv");
	const auto& entries = db.getEntries(mediaType::kAnime);
	REQUIRE(entries.size() == 1);
	CHECK(entries[0].title == "Odd Taxi");
	CHECK(catalist::formatDate(entries[0].updatedAt) == "2026-10-17");
	CHECK(entries[0].malId == 44265);
}

TEST_CASE_FIXTURE(tempDirFixture, "csv loads quoted fields that span lines") {
	catalogue db(tempDir());
	loadFixture(db, mediaType::kAnime, "multiline.csv");
	const auto& entries = db.getEntries(mediaType::kAnime);
	REQUIRE(entries.size() == 2);
	CHECK(entries[0].title == "Ghost, \"in\" the Shell");
	CHECK(entries[0].status == watchStatus::kDropped);
	CHECK(catalist::formatDate(entries[0].updatedAt) == "2026-10-18");
	CHECK(entries[1].title == "Next");
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
	int id = db.addEntry(makeEntry().title("Kept, \"quoted\"").rewatched(2));
	db.addEntry(makeEntry().title("Paper").type(mediaType::kManga));
	REQUIRE(db.saveAll());
	CHECK(std::filesystem::exists(tempDir() / "data/anime.csv"));
	CHECK(std::filesystem::exists(tempDir() / "data/manga.csv"));

	catalogue reloaded(tempDir() / "data");
	REQUIRE(reloaded.loadAll().value());
	CHECK(reloaded.getById(mediaType::kAnime, id)->title == "Kept, \"quoted\"");
	CHECK(reloaded.getById(mediaType::kAnime, id)->rewatched == 2);
	CHECK(reloaded.getEntries(mediaType::kManga)[0].title == "Paper");
	CHECK(reloaded.getEntries(mediaType::kManga)[0].type == mediaType::kManga);
}

TEST_CASE_FIXTURE(tempDirFixture, "catalogue reports a failed write") {
	catalogue db(tempDir() / "data");
	catalist::test::makeUnwritable(tempDir() / "data/manga.csv");
	CHECK_FALSE(db.saveAll());
	CHECK(catalist::test::readText(tempDir() / "data/manga.csv").empty());
}

TEST_CASE("catalogue data dir comes from CATALIST_DATA_DIR first") {
	// The test main points CATALIST_DATA_DIR at a sandbox.
	CHECK(catalogue::getDefaultConfigDir() == std::getenv("CATALIST_DATA_DIR"));
	catalogue db("/data");
	CHECK(db.getConfigPath() == "/data");
	CHECK(db.getCsvPath(mediaType::kAnime) == "/data/anime.csv");
	CHECK(db.getCsvPath(mediaType::kManga) == "/data/manga.csv");
}
