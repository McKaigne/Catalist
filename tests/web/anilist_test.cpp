#include "web/anilist.hpp"

#include <doctest/doctest.h>

#include "support/test_support.hpp"
#include "web/json.hpp"

namespace anilist = catalist::anilist;
using catalist::MediaType;

namespace {

// Real replies, shortened: Frieren (finished), One Piece (airing) and
// Berserk (a manga still running).
const char* kFrieren =
    R"({"data":{"Media":{"title":{"romaji":"Sousou no Frieren"},"episodes":28,"chapters":null,"volumes":null,
  "status":"FINISHED","nextAiringEpisode":null}}})";
const char* kOnePiece = R"({"data":{"Media":{"episodes":null,"chapters":null,"volumes":null,
  "status":"RELEASING","nextAiringEpisode":{"episode":1181}}}})";
const char* kBerserk = R"({"data":{"Media":{"episodes":null,"chapters":null,"volumes":null,
  "status":"RELEASING"}}})";

}  // namespace

TEST_CASE("anilist reads a finished title and its totals") {
	auto found = anilist::ParseLookup(kFrieren, MediaType::kAnime);
	REQUIRE(found.has_value());
	CHECK(found.value().name == "Sousou no Frieren");
	CHECK(found.value().facts.total_units == 28);
	CHECK(found.value().facts.released == 28);
	CHECK_FALSE(found.value().ongoing);
}

TEST_CASE("anilist counts the aired episodes from the next one to air") {
	auto found = anilist::ParseLookup(kOnePiece, MediaType::kAnime);
	REQUIRE(found.has_value());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.total_units == 0);
	CHECK(found.value().facts.released == 1180);
}

TEST_CASE("anilist leaves a running manga's chapters unknown") {
	auto found = anilist::ParseLookup(kBerserk, MediaType::kManga);
	REQUIRE(found.has_value());
	CHECK(found.value().ongoing);
	CHECK(found.value().facts.released == 0);
	CHECK_FALSE(anilist::ParseLookup(R"({"data":{"Media":null}})", MediaType::kManga).has_value());
	CHECK_FALSE(anilist::ParseLookup("<html>", MediaType::kManga).has_value());
}

TEST_CASE("anilist asks by its own id when known, else by the MyAnimeList id") {
	auto by_mal = anilist::LookupRequest({MediaType::kManga, 0, 2});
	CHECK(by_mal.find("Media(idMal:$id") != std::string::npos);
	CHECK(by_mal.find(R"("variables":{"id":2,"type":"MANGA"})") != std::string::npos);
	auto own = anilist::LookupRequest({MediaType::kAnime, 154587, 52991});
	CHECK(own.find("Media(id:$id") != std::string::npos);
	CHECK(own.find(R"("id":154587)") != std::string::npos);
	CHECK(catalist::json::Parse(own).has_value());

	catalist::test::FakeWeb web;
	// Without an id there is nothing to ask.
	anilist::Tracker site(web.Link());
	CHECK_FALSE(site.LookUp(catalist::Entry{}).has_value());
	CHECK(web.sent.empty());
}

TEST_CASE("anilist reads both ids from a lookup") {
	auto found = anilist::ParseLookup(R"({"data":{"Media":{"id":154587,"idMal":52991}}})",
	                                  MediaType::kAnime);
	REQUIRE(found.has_value());
	CHECK(found.value().facts.anilist_id == 154587);
	CHECK(found.value().facts.mal_id == 52991);
}

TEST_CASE("anilist escapes the search") {
	auto search = anilist::SearchRequest(MediaType::kAnime, "say \"hi\"\\\n");
	CHECK(search.find(R"("search":"say \"hi\"\\\u000a")") != std::string::npos);
	CHECK(catalist::json::Parse(search).has_value());
}

TEST_CASE("anilist search keeps titles MyAnimeList does not list") {
	auto found = anilist::ParseSearch(
	    R"({"data":{"Page":{"media":[
	      {"id":5114,"idMal":5114,"title":{"romaji":"Fullmetal Alchemist: Brotherhood"},
	       "episodes":64,"averageScore":90,"startDate":{"year":2009}},
	      {"id":999001,"idMal":null,"title":{"romaji":"AniList only"},"episodes":3},
	      {"id":null,"title":{"romaji":"No id at all"}}]}}})",
	    MediaType::kAnime);
	REQUIRE(found.has_value());
	REQUIRE(found.value().size() == 2);
	CHECK(found.value()[0].anilist_id == 5114);
	CHECK(found.value()[0].mal_id == 5114);
	CHECK(found.value()[0].total_units == 64);
	CHECK(found.value()[0].year == "2009");
	CHECK(found.value()[0].score == doctest::Approx(9.0));
	CHECK(found.value()[1].anilist_id == 999001);
	CHECK(found.value()[1].mal_id == 0);
	CHECK(found.value()[1].year.empty());
	CHECK(found.value()[1].score == 0.0);
}

TEST_CASE("anilist looks up many titles in one request, by either id") {
	auto request = anilist::BatchRequest(MediaType::kAnime, {154587, 128547}, false);
	CHECK(request.find("media(id_in:$ids") != std::string::npos);
	CHECK(request.find(R"("ids":[154587,128547],"type":"ANIME")") != std::string::npos);
	CHECK(catalist::json::Parse(request).has_value());
	CHECK(anilist::BatchRequest(MediaType::kManga, {2}, true).find("idMal_in:$ids") !=
	      std::string::npos);

	catalist::test::FakeWeb web;
	web.Respond(R"("ids":[154587,128547],"type":"ANIME")",
	            R"({"data":{"Page":{"media":[
	            {"id":128547,"idMal":44265,"episodes":13,"status":"FINISHED"},
	            {"id":154587,"idMal":52991,"episodes":28,"status":"FINISHED"}]}}})");
	web.Respond(R"("ids":[2],"type":"MANGA")",
	            R"({"data":{"Page":{"media":[{"id":30002,"idMal":2,"status":"RELEASING"}]}}})");
	int pauses = 0;
	auto net = web.Link();
	net.pause = [&pauses] { ++pauses; };
	anilist::Tracker site(net);
	std::vector<catalist::Entry> titles = {
	    catalist::test::MakeEntry().AnilistId(154587),
	    catalist::test::MakeEntry().Type(MediaType::kManga).MalId(2),
	    catalist::test::MakeEntry().AnilistId(128547),
	    catalist::test::MakeEntry().Title("Home Video"),
	};
	auto found = site.LookUpMany(titles);
	REQUIRE(found.size() == 4);
	CHECK(found[0].value().facts.total_units == 28);
	CHECK(found[1].value().facts.anilist_id == 30002);
	CHECK(found[2].value().facts.total_units == 13);
	CHECK(found[3].error().message == "the title has no id to look up");
	// One request for each list, a pause between them.
	CHECK(web.sent.size() == 2);
	CHECK(pauses == 1);
}

TEST_CASE("anilist many-title lookups fail as one when AniList cannot be reached") {
	catalist::test::FakeWeb web;
	anilist::Tracker site(web.Link());
	auto found =
	    site.LookUpMany({catalist::test::MakeEntry().AnilistId(1),
	                     catalist::test::MakeEntry().Type(MediaType::kManga).AnilistId(2)});
	REQUIRE(found.size() == 2);
	CHECK(found[0].error().message == "could not reach AniList");
	CHECK(found[1].error().message == "could not reach AniList");
	// Once it is not there, it is not asked again.
	CHECK(web.sent.size() == 1);
}
