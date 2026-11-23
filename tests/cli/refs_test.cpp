#include "cli/refs.hpp"

#include <doctest/doctest.h>

#include "support/test_support.hpp"

namespace {

using catalist::MediaType;
using catalist::test::MakeEntry;
namespace refs = catalist::refs;

// Anime a:1 Sousou no Frieren, a:2 Odd Taxi, a:3 Frieren; manga m:1 Berserk,
// m:2 Sousou no Frieren.
struct RefsFixture : catalist::test::TempDirFixture {
	RefsFixture() : db(TempDir()) {
		db.AddEntry(MakeEntry().Title("Sousou no Frieren"));
		db.AddEntry(MakeEntry().Title("Odd Taxi"));
		db.AddEntry(MakeEntry().Title("Frieren"));
		db.AddEntry(MakeEntry().Title("Berserk").Type(MediaType::kManga));
		db.AddEntry(MakeEntry().Title("Sousou no Frieren").Type(MediaType::kManga));
	}

	// The refs of what the text names, in order.
	std::vector<std::string> Found(const std::string& text,
	                               std::optional<MediaType> only = std::nullopt) {
		std::vector<std::string> all;
		for (const auto& each : refs::Find(db, text, only))
			all.push_back(refs::Of(each));
		return all;
	}

	catalist::Catalogue db;
};

using List = std::vector<std::string>;

}  // namespace

TEST_CASE("refs reads a:N and m:N, and nothing else, as a ref") {
	auto anime = refs::Parse("a:12");
	REQUIRE(anime.has_value());
	CHECK(anime->type == MediaType::kAnime);
	CHECK(anime->id == 12);
	auto manga = refs::Parse("m:3");
	REQUIRE(manga.has_value());
	CHECK(manga->type == MediaType::kManga);
	CHECK(manga->id == 3);
	CHECK(refs::Parse("a:007")->id == 7);

	for (const char* text : {"", "a", "a:", "12", "x:12", "A:12", "a:1x", "a:-1", "a: 1", "a:1 ",
	                         "anime:12", "a:12345678901"})
		CHECK_FALSE(refs::Parse(text).has_value());
}

TEST_CASE("refs writes the ref that it reads") {
	CHECK(refs::Of(MediaType::kAnime, 12) == "a:12");
	CHECK(refs::Of(MediaType::kManga, 3) == "m:3");
	catalist::Entry manga = MakeEntry().Type(MediaType::kManga);
	manga.id = 40;
	CHECK(refs::Of(manga) == "m:40");
	CHECK(refs::Parse(refs::Of(manga))->id == 40);
}

TEST_CASE_FIXTURE(RefsFixture, "refs finds a title by its ref") {
	CHECK(Found("a:2") == List{"a:2"});
	CHECK(Found("m:1") == List{"m:1"});
	CHECK(Found("a:9").empty());
	CHECK(Found("a:0").empty());
	// The ref of the other list names nothing in this one.
	CHECK(Found("a:2", MediaType::kManga).empty());
	CHECK(Found("a:2", MediaType::kAnime) == List{"a:2"});
}

TEST_CASE_FIXTURE(RefsFixture, "refs finds titles by a part of them, in any case") {
	CHECK(Found("taxi") == List{"a:2"});
	CHECK(Found("ODD T") == List{"a:2"});
	CHECK(Found("sousou") == List{"a:1", "m:2"});
	CHECK(Found("sousou", MediaType::kManga) == List{"m:2"});
	CHECK(Found("ser") == List{"m:1"});
	CHECK(Found("naruto").empty());
	CHECK(Found("frieren sousou") == List{"a:1", "m:2"});
	CHECK(Found("taxi naruto").empty());
}

TEST_CASE_FIXTURE(RefsFixture, "refs lets a title that is exactly the text win") {
	// "frieren" is in three titles and is one of them.
	CHECK(Found("frieren") == List{"a:3"});
	CHECK(Found("Sousou no Frieren") == List{"a:1", "m:2"});
	CHECK(Found("frieren", MediaType::kManga) == List{"m:2"});
	CHECK(Found("friere") == List{"a:1", "a:3", "m:2"});
}

TEST_CASE("refs matches a title by any part of it, in any case") {
	catalist::Entry shown = MakeEntry().Title("Sousou no Frieren");
	CHECK(refs::TitleHolds(shown, "no fri"));
	CHECK(refs::TitleHolds(shown, "SOUSOU"));
	CHECK(refs::TitleHolds(shown, ""));
	CHECK(refs::TitleHolds(shown, "frieren sousou"));
	CHECK_FALSE(refs::TitleHolds(shown, "frieren taxi"));
	catalist::Entry kanji = MakeEntry().Title("葬送のフリーレン");
	CHECK(refs::TitleHolds(kanji, "フリーレン"));
}

TEST_CASE("refs matches the English name too, but each name on its own") {
	catalist::Entry shown =
	    MakeEntry().Title("Sousou no Frieren").EnglishTitle("Frieren: Beyond Journey's End");
	CHECK(refs::TitleHolds(shown, "sousou"));
	CHECK(refs::TitleHolds(shown, "frieren beyond journeys end"));
	CHECK(refs::TitleHolds(shown, "END, journey's"));
	CHECK_FALSE(refs::TitleHolds(shown, "sousou beyond"));
}

TEST_CASE_FIXTURE(RefsFixture, "refs finds the closest title to a typo") {
	CHECK(DisplayName(*refs::Closest(db, "freiren", std::nullopt)) == "Sousou no Frieren");
	CHECK(DisplayName(*refs::Closest(db, "bersrek", std::nullopt)) == "Berserk");
	CHECK_FALSE(refs::Closest(db, "bersrek", MediaType::kAnime).has_value());
	CHECK_FALSE(refs::Closest(db, "naruto", std::nullopt).has_value());
	CHECK_FALSE(refs::Closest(db, "", std::nullopt).has_value());
}
