#include "storage/catalogue.hpp"

#include <doctest/doctest.h>

#include <filesystem>

#include "storage/csv_codec.hpp"
#include "support/test_support.hpp"

using catalist::Catalogue;
using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::MakeEntry;
using catalist::test::ReadText;
using catalist::test::TempDirFixture;
using catalist::test::WriteText;

namespace {

const char* const kStamp = "2026-11-24 10:00:00";

bool Has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

}  // namespace

TEST_CASE_FIXTURE(TempDirFixture, "catalogue saves both lists to one csv and reads them back") {
	auto file = TempDir() / "catalist.csv";
	Catalogue db(file);
	CHECK(db.Load().value() == false);
	CHECK_FALSE(std::filesystem::exists(file));
	db.AddEntry(MakeEntry()
	                .MalId(7)
	                .AnilistId(70)
	                .Title("Kept, \"quoted\"")
	                .EnglishTitle("English")
	                .Status(WatchStatus::kCompleted)
	                .Score(6)
	                .Progress(3)
	                .TotalUnits(12)
	                .StartDate("2026-01-02")
	                .FinishDate("2026-02-03"));
	db.AddEntry(MakeEntry().Title("Paper").Type(MediaType::kManga).Volumes(3).TotalVolumes(9));
	REQUIRE(db.Save("add", kStamp));

	auto text = ReadText(file);
	CHECK(text.starts_with(std::string(catalist::csv_codec::kHeader) + "\n"));
	CHECK(Has(text, "1,anime,\"Kept, \"\"quoted\"\"\",English,completed,6,3,12,"));
	CHECK(Has(text, "1,manga,Paper,"));
	CHECK_FALSE(std::filesystem::exists(TempDir() / "catalist.csv.tmp"));

	Catalogue again(file);
	CHECK(again.Load().value());
	CHECK(again.GetEntries(MediaType::kAnime) == db.GetEntries(MediaType::kAnime));
	CHECK(again.GetEntries(MediaType::kManga) == db.GetEntries(MediaType::kManga));
	CHECK(again.GetById(MediaType::kAnime, 1)->changed_at == kStamp);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue refuses a damaged file and leaves it alone") {
	auto file = TempDir() / "catalist.csv";
	for (const std::string& text :
	     {std::string("not a list\n"),
	      std::string(catalist::csv_codec::kHeader) + "\n1,anime,T,,planned\n",
	      std::string(catalist::csv_codec::kHeader) +
	          "\n1,anime,T,,planned,0,0,0,0,0,0,,,,,0,0\n1,anime,U,,planned,0,0,0,0,0,0,,,,,0,0\n"}) {
		WriteText(file, text);
		Catalogue db(file);
		CHECK_FALSE(db.Load().has_value());
		CHECK(ReadText(file) == text);
	}
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue goes back to what the file holds when a save fails") {
	Catalogue db(TempDir() / "no-such-dir" / "catalist.csv");
	db.AddEntry(MakeEntry().Title("Nowhere to go"));
	CHECK_FALSE(db.Save("add", kStamp));
	CHECK(db.GetEntries(MediaType::kAnime).empty());
	CHECK(db.History().empty());
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue merges a title known by either site id") {
	Catalogue db(TempDir() / "catalist.csv");
	int id = db.AddEntry(MakeEntry().MalId(100).Title("Blank").Status(WatchStatus::kCompleted));
	CHECK(db.AddEntry(MakeEntry().MalId(100).Title("Other").Score(7).FinishDate("2024-01-01")) == id);
	REQUIRE(db.GetEntries(MediaType::kAnime).size() == 1);
	auto merged = *db.GetById(MediaType::kAnime, id);
	CHECK(merged.title == "Blank");
	CHECK(merged.score == 7);
	CHECK(catalist::FormatDate(merged.finish_date) == "2024-01-01");
	CHECK(db.FindByIds(MediaType::kAnime, 0, 100).has_value());
	CHECK_FALSE(db.FindByIds(MediaType::kManga, 0, 100).has_value());
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue imports a title without ids once, by its name") {
	Catalogue db(TempDir() / "catalist.csv");
	db.ImportEntry(MakeEntry().Title("No Id"));
	db.ImportEntry(MakeEntry().Title("no id").Score(5));
	REQUIRE(db.GetEntries(MediaType::kAnime).size() == 1);
	CHECK(db.GetEntries(MediaType::kAnime)[0].score == 5);
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue stamps only the titles a save changed") {
	Catalogue db(TempDir() / "catalist.csv");
	db.AddEntry(MakeEntry().Title("A"));
	db.AddEntry(MakeEntry().Title("B"));
	REQUIRE(db.Save("add", "2026-11-24 10:00:00"));
	auto b = *db.GetById(MediaType::kAnime, 2);
	b.score = 9;
	db.UpdateEntry(b);
	REQUIRE(db.Save("score", "2026-11-24 11:00:00"));
	CHECK(db.GetById(MediaType::kAnime, 1)->changed_at == "2026-11-24 10:00:00");
	CHECK(db.GetById(MediaType::kAnime, 2)->changed_at == "2026-11-24 11:00:00");
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue undoes and redoes, and the file follows") {
	auto file = TempDir() / "catalist.csv";
	Catalogue db(file);
	CHECK_FALSE(db.Undo().value().has_value());
	db.AddEntry(MakeEntry().Title("Taxi"));
	REQUIRE(db.Save("add", kStamp));
	auto taxi = *db.GetById(MediaType::kAnime, 1);
	taxi.score = 7;
	db.UpdateEntry(taxi);
	REQUIRE(db.Save("score", kStamp));

	CHECK(db.Undo().value() == "score");
	CHECK(db.GetById(MediaType::kAnime, 1)->score == 0);
	Catalogue read(file);
	REQUIRE(read.Load().value());
	CHECK(read.GetById(MediaType::kAnime, 1)->score == 0);
	auto steps = db.History();
	REQUIRE(steps.size() == 2);
	CHECK(steps[0].label == "score");
	CHECK(steps[0].undone);
	CHECK_FALSE(steps[1].undone);

	CHECK(db.Redo().value() == "score");
	CHECK(db.GetById(MediaType::kAnime, 1)->score == 7);
	CHECK_FALSE(db.Redo().value().has_value());

	REQUIRE(db.Undo().has_value());
	db.RemoveEntry(MediaType::kAnime, 1);
	REQUIRE(db.Save("remove", kStamp));
	CHECK_FALSE(db.Redo().value().has_value());
	CHECK(db.Undo().value() == "remove");
	CHECK(db.GetById(MediaType::kAnime, 1).has_value());
}

TEST_CASE_FIXTURE(TempDirFixture, "catalogue keeps the last 100 steps") {
	Catalogue db(TempDir() / "catalist.csv");
	db.AddEntry(MakeEntry().Title("Taxi"));
	for (size_t i = 1; i <= Catalogue::kHistoryLength + 5; ++i) {
		auto taxi = *db.GetById(MediaType::kAnime, 1);
		taxi.progress = static_cast<int>(i);
		db.UpdateEntry(taxi);
		REQUIRE(db.Save(std::to_string(i), kStamp));
	}
	CHECK(db.History().size() == Catalogue::kHistoryLength);
}
