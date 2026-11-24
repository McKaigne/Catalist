#include "menu/find.hpp"

#include <doctest/doctest.h>

#include <format>

#include "support/test_support.hpp"

namespace {

using catalist::MediaType;
using catalist::test::MakeEntry;

struct FindFixture : catalist::test::TempDirFixture {
	FindFixture() : db(TempDir() / "catalist.csv") {
		db.AddEntry(MakeEntry().Title("Sousou no Frieren"));
		db.AddEntry(MakeEntry().Title("Odd Taxi"));
		db.AddEntry(MakeEntry().Title("Frieren"));
		db.AddEntry(MakeEntry().Title("Berserk").Type(MediaType::kManga));
		db.AddEntry(MakeEntry().Title("Sousou no Frieren").Type(MediaType::kManga));
	}

	std::vector<std::string> Found(const std::string& text) {
		std::vector<std::string> names;
		for (const auto& each : catalist::FindTitles(db, text))
			names.push_back(std::format("{} {}", catalist::TypeKey(each.type), each.id));
		return names;
	}

	catalist::Catalogue db;
};

using List = std::vector<std::string>;

}  // namespace

TEST_CASE_FIXTURE(FindFixture, "find takes any part of a title, words in any order") {
	CHECK(Found("taxi") == List{"anime 2"});
	CHECK(Found("TAXI odd") == List{"anime 2"});
	CHECK(Found("frieren sousou") == List{"anime 1", "manga 2"});
	CHECK(Found("ser") == List{"manga 1"});
	CHECK(Found("naruto").empty());
	CHECK(Found("!!").empty());
}

TEST_CASE_FIXTURE(FindFixture, "find lets a title that is exactly the text win") {
	CHECK(Found("frieren") == List{"anime 3"});
	CHECK(Found("friere") == List{"anime 1", "anime 3", "manga 2"});
}

TEST_CASE("find matches the English name too, but each name on its own") {
	catalist::Entry shown =
	    MakeEntry().Title("Sousou no Frieren").EnglishTitle("Frieren: Beyond Journey's End");
	CHECK(catalist::TitleHolds(shown, "sousou"));
	CHECK(catalist::TitleHolds(shown, "frieren beyond journeys end"));
	CHECK(catalist::TitleHolds(shown, "END, journey's"));
	CHECK_FALSE(catalist::TitleHolds(shown, "sousou beyond"));
}

TEST_CASE_FIXTURE(FindFixture, "find offers the closest title to a typo") {
	CHECK(catalist::DisplayName(*catalist::ClosestTitle(db, "bersrek")) == "Berserk");
	CHECK(catalist::DisplayName(*catalist::ClosestTitle(db, "freiren")) == "Sousou no Frieren");
	CHECK_FALSE(catalist::ClosestTitle(db, "naruto").has_value());
	CHECK_FALSE(catalist::ClosestTitle(db, "").has_value());
}
