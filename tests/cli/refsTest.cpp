#include <doctest/doctest.h>

#include "cli/refs.hpp"
#include "support/testSupport.hpp"

namespace {

using catalist::mediaType;
using catalist::test::makeEntry;
namespace refs = catalist::refs;

// Anime a:1 Sousou no Frieren, a:2 Odd Taxi, a:3 Frieren; manga m:1 Berserk,
// m:2 Sousou no Frieren.
struct refsFixture : catalist::test::tempDirFixture {
	refsFixture() : db(tempDir()) {
		db.addEntry(makeEntry().title("Sousou no Frieren"));
		db.addEntry(makeEntry().title("Odd Taxi"));
		db.addEntry(makeEntry().title("Frieren"));
		db.addEntry(makeEntry().title("Berserk").type(mediaType::kManga));
		db.addEntry(makeEntry().title("Sousou no Frieren").type(mediaType::kManga));
	}

	// The refs of what the text names, in order.
	std::vector<std::string> found(const std::string& text,
	                               std::optional<mediaType> only = std::nullopt) {
		std::vector<std::string> all;
		for (const auto& each : refs::find(db, text, only))
			all.push_back(refs::of(each));
		return all;
	}

	catalist::catalogue db;
};

using list = std::vector<std::string>;

}  // namespace

TEST_CASE("refs reads a:N and m:N, and nothing else, as a ref") {
	auto anime = refs::parse("a:12");
	REQUIRE(anime.has_value());
	CHECK(anime->type == mediaType::kAnime);
	CHECK(anime->id == 12);
	auto manga = refs::parse("m:3");
	REQUIRE(manga.has_value());
	CHECK(manga->type == mediaType::kManga);
	CHECK(manga->id == 3);
	CHECK(refs::parse("a:007")->id == 7);

	for (const char* text : {"", "a", "a:", "12", "x:12", "A:12", "a:1x", "a:-1", "a: 1", "a:1 ",
	                         "anime:12", "a:12345678901"})
		CHECK_FALSE(refs::parse(text).has_value());
}

TEST_CASE("refs writes the ref that it reads") {
	CHECK(refs::of(mediaType::kAnime, 12) == "a:12");
	CHECK(refs::of(mediaType::kManga, 3) == "m:3");
	catalist::entry manga = makeEntry().type(mediaType::kManga);
	manga.id = 40;
	CHECK(refs::of(manga) == "m:40");
	CHECK(refs::parse(refs::of(manga))->id == 40);
}

TEST_CASE_FIXTURE(refsFixture, "refs finds a title by its ref") {
	CHECK(found("a:2") == list{"a:2"});
	CHECK(found("m:1") == list{"m:1"});
	CHECK(found("a:9").empty());
	CHECK(found("a:0").empty());
	// The ref of the other list names nothing in this one.
	CHECK(found("a:2", mediaType::kManga).empty());
	CHECK(found("a:2", mediaType::kAnime) == list{"a:2"});
}

TEST_CASE_FIXTURE(refsFixture, "refs finds titles by a part of them, in any case") {
	CHECK(found("taxi") == list{"a:2"});
	CHECK(found("ODD T") == list{"a:2"});
	CHECK(found("sousou") == list{"a:1", "m:2"});
	CHECK(found("sousou", mediaType::kManga) == list{"m:2"});
	CHECK(found("ser") == list{"m:1"});
	CHECK(found("naruto").empty());
	// The words must be there as written, not each somewhere.
	CHECK(found("sousou frieren").empty());
}

TEST_CASE_FIXTURE(refsFixture, "refs lets a title that is exactly the text win") {
	// "frieren" is in three titles and is one of them.
	CHECK(found("frieren") == list{"a:3"});
	CHECK(found("Sousou no Frieren") == list{"a:1", "m:2"});
	CHECK(found("frieren", mediaType::kManga) == list{"m:2"});
	CHECK(found("friere") == list{"a:1", "a:3", "m:2"});
}

TEST_CASE("refs matches a title by any part of it, in any case") {
	catalist::entry shown = makeEntry().title("Sousou no Frieren");
	CHECK(refs::titleHolds(shown, "no fri"));
	CHECK(refs::titleHolds(shown, "SOUSOU"));
	CHECK(refs::titleHolds(shown, ""));
	CHECK_FALSE(refs::titleHolds(shown, "frieren sousou"));
	catalist::entry kanji = makeEntry().title("葬送のフリーレン");
	CHECK(refs::titleHolds(kanji, "フリーレン"));
}
