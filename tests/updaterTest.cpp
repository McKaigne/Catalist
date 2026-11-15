#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "support/testSupport.hpp"
#include "updater.hpp"

using catalist::mediaType;
using catalist::updater;
using catalist::watchStatus;
using catalist::anilist::titleRef;
using catalist::test::fakeWeb;
using catalist::test::makeEntry;

namespace {

// Berserk, known to the catalogue by its MyAnimeList id (2) only; AniList's
// own id for it is 30002.
const titleRef kBerserk{mediaType::kManga, 0, 2};
const char* kFinishedManga = R"({"data":{"Media":{"id":30002,"idMal":2,
  "title":{"romaji":"Berserk"},"chapters":380,"volumes":42,"status":"FINISHED",
  "coverImage":{"large":"https://img/berserk.jpg"}}}})";

const char* kAiringAnime = R"({"data":{"Media":{"id":21,"idMal":21,
  "episodes":null,"status":"RELEASING","nextAiringEpisode":{"episode":1181},
  "coverImage":{"large":"https://img/op.jpg"}}}})";

// The request body AniList gets for the title, which the fake web answers.
std::string asks(const titleRef& title) {
	return catalist::anilist::lookupRequest(title);
}

titleRef anime(int anilistId) {
	return {mediaType::kAnime, anilistId, 0};
}

// A fake converter that shows what it was given.
struct fakeSixel {
	std::optional<std::string> operator()(const std::string& image, int widthPx, int heightPx) {
		++calls;
		return "SIXEL[" + image + " " + std::to_string(widthPx) + "x" + std::to_string(heightPx) +
		       "]";
	}
	std::atomic<int> calls{0};
};

struct updaterFixture {
	updaterFixture()
	    : web(),
	      updates(
	          web.fetcher(), web.poster(), catalist::test::testToday,
	          [this](const std::string& image, int w, int h) { return sixel(image, w, h); },
	          std::chrono::milliseconds(0)) {}

	size_t downloadsOf(const std::string& fragment) {
		return static_cast<size_t>(std::count_if(
		    web.requested.begin(), web.requested.end(),
		    [&](const std::string& url) { return url.find(fragment) != std::string::npos; }));
	}

	size_t lookupsOf(const titleRef& title) {
		return static_cast<size_t>(std::count(web.posted.begin(), web.posted.end(), asks(title)));
	}

	fakeWeb web;
	fakeSixel sixel;
	updater updates;
};

}  // namespace

TEST_CASE_FIXTURE(updaterFixture, "updater looks a title up once per run and learns its ids") {
	web.respond(asks(kBerserk), kFinishedManga);
	updates.lookUp(kBerserk);
	updates.lookUp(kBerserk);
	updates.lookUp({mediaType::kManga, 0, 0});
	updates.waitUntilIdle();

	auto findings = updates.takeFindings();
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].asked.type == mediaType::kManga);
	CHECK(findings[0].asked.malId == 2);
	CHECK(findings[0].facts.anilistId == 30002);
	CHECK(findings[0].facts.malId == 2);
	CHECK(findings[0].facts.totalUnits == 380);
	CHECK(findings[0].facts.totalVolumes == 42);
	CHECK(findings[0].facts.released == 380);
	CHECK(web.posted.size() == 1);
	CHECK(updates.takeFindings().empty());

	// Asked again by its AniList id, it is the same title: no new request.
	updates.lookUp({mediaType::kManga, 30002, 2});
	updates.waitUntilIdle();
	CHECK(web.posted.size() == 1);
}

TEST_CASE_FIXTURE(updaterFixture, "updater counts the episodes an airing anime has out") {
	web.respond(asks(anime(21)), kAiringAnime);
	updates.lookUp(anime(21));
	updates.waitUntilIdle();

	auto findings = updates.takeFindings();
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].facts.totalUnits == 0);
	CHECK(findings[0].facts.released == 1180);
}

TEST_CASE_FIXTURE(updaterFixture, "updater finds nothing when AniList cannot be reached") {
	updates.lookUp(anime(5));
	updates.waitUntilIdle();
	CHECK(updates.takeFindings().empty());
	CHECK(lookupsOf(anime(5)) == 1);
}

TEST_CASE_FIXTURE(updaterFixture, "updater fetches a cover once and converts it per size") {
	web.respond(asks(kBerserk), kFinishedManga);
	web.respond("img/berserk.jpg", "JPEG");
	CHECK(updates.coverFor(kBerserk, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	auto ready = updates.coverFor(kBerserk, 70, 100);
	CHECK(ready.state == updater::coverState::kReady);
	CHECK(ready.sixel == "SIXEL[JPEG 70x100]");

	// Once the entry knows its AniList id, the cover is still the same one.
	CHECK(updates.coverFor({mediaType::kManga, 30002, 2}, 70, 100).sixel == "SIXEL[JPEG 70x100]");

	// Another size converts again, but the image is not fetched again.
	CHECK(updates.coverFor(kBerserk, 140, 200).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(kBerserk, 140, 200).sixel == "SIXEL[JPEG 140x200]");
	CHECK(downloadsOf("berserk.jpg") == 1);
	CHECK(lookupsOf(kBerserk) == 1);
	CHECK(sixel.calls == 2);
	// Covers never change the catalogue.
	CHECK(updates.takeFindings().empty());
}

TEST_CASE_FIXTURE(updaterFixture, "updater reuses the cover address from a lookup") {
	web.respond(asks(kBerserk), kFinishedManga);
	web.respond("img/berserk.jpg", "JPEG");
	updates.lookUp(kBerserk);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(kBerserk, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(kBerserk, 70, 100).state == updater::coverState::kReady);
	CHECK(lookupsOf(kBerserk) == 1);
}

TEST_CASE_FIXTURE(updaterFixture, "updater says why there is no cover") {
	CHECK(updates.coverFor({mediaType::kAnime, 0, 0}, 70, 100).state == updater::coverState::kNone);

	// AniList answers but has no picture.
	web.respond(asks(anime(8)), R"({"data":{"Media":{"id":8,"coverImage":{"large":null}}}})");
	CHECK(updates.coverFor(anime(8), 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(anime(8), 70, 100).state == updater::coverState::kNone);

	// Offline: unreachable, and not asked for again until the wait is over.
	CHECK(updates.coverFor(anime(7), 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(anime(7), 70, 100).state == updater::coverState::kUnreachable);
	CHECK(updates.coverFor(anime(7), 140, 200).state == updater::coverState::kUnreachable);
	CHECK(lookupsOf(anime(7)) == 1);
}

TEST_CASE("updater tries an unreachable cover again after a while") {
	fakeWeb web;
	updater updates(
	    web.fetcher(), web.poster(), catalist::test::testToday,
	    [](const std::string& image, int, int) { return std::optional<std::string>(image); },
	    std::chrono::milliseconds(0), {}, std::chrono::milliseconds(50));
	CHECK(updates.coverFor(anime(7), 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(anime(7), 70, 100).state == updater::coverState::kUnreachable);

	// Back online (AniList was busy, say): the next ask after the wait works.
	web.respond(asks(anime(7)), R"({"data":{"Media":{"id":7,
      "coverImage":{"large":"https://img/7.jpg"}}}})");
	web.respond("img/7.jpg", "JPEG");
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
	CHECK(updates.coverFor(anime(7), 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	auto ready = updates.coverFor(anime(7), 70, 100);
	CHECK(ready.state == updater::coverState::kReady);
	CHECK(ready.sixel == "JPEG");
}

TEST_CASE("updater says when the picture cannot be converted") {
	fakeWeb web;
	web.respond(asks(kBerserk), kFinishedManga);
	web.respond("img/berserk.jpg", "not really a JPEG");
	updater updates(
	    web.fetcher(), web.poster(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>(); },
	    std::chrono::milliseconds(0));
	CHECK(updates.coverFor(kBerserk, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(kBerserk, 70, 100).state == updater::coverState::kUnreadable);
}

TEST_CASE_FIXTURE(updaterFixture, "updater looks a title up again when AniList was unreachable") {
	updates.lookUp(kBerserk);
	updates.waitUntilIdle();
	CHECK(updates.takeFindings().empty());
	web.respond(asks(kBerserk), kFinishedManga);
	updates.lookUp(kBerserk);
	updates.waitUntilIdle();
	CHECK(updates.takeFindings().size() == 1);
	CHECK(lookupsOf(kBerserk) == 2);
}

TEST_CASE("updater pauses between requests") {
	fakeWeb web;
	updater updates(
	    web.fetcher(), web.poster(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>(); },
	    std::chrono::milliseconds(60));
	auto start = std::chrono::steady_clock::now();
	updates.lookUp(anime(1));
	updates.lookUp(anime(2));
	updates.lookUp(anime(3));
	updates.waitUntilIdle();
	auto elapsed = std::chrono::steady_clock::now() - start;
	CHECK(web.posted.size() == 3);
	// The first request goes at once; each later one waits its turn.
	CHECK(elapsed >= std::chrono::milliseconds(120));
}

TEST_CASE_FIXTURE(catalist::test::tempDirFixture,
                  "checkFollowed asks only for followed titles not checked today") {
	catalist::catalogue lists(tempDir());
	lists.addEntry(makeEntry().malId(1).status(watchStatus::kWatching));
	lists.addEntry(makeEntry().anilistId(2).status(watchStatus::kOnHold));
	lists.addEntry(makeEntry().malId(3).status(watchStatus::kCompleted));
	lists.addEntry(makeEntry().malId(4).status(watchStatus::kPlanToWatch));
	lists.addEntry(makeEntry().status(watchStatus::kWatching));
	lists.addEntry(makeEntry().malId(6).status(watchStatus::kWatching).checkedAt("2026-10-20"));
	lists.addEntry(makeEntry().malId(7).status(watchStatus::kWatching).checkedAt("2026-10-19"));
	lists.addEntry(makeEntry().malId(8).type(mediaType::kManga).status(watchStatus::kWatching));

	fakeWeb web;
	updater updates(
	    web.fetcher(), web.poster(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>(); },
	    std::chrono::milliseconds(0));
	catalist::checkFollowed(lists, updates, catalist::test::testToday());
	updates.waitUntilIdle();
	std::vector<std::string> sent = web.posted;
	std::vector<std::string> expected = {asks({mediaType::kAnime, 0, 1}), asks(anime(2)),
	                                     asks({mediaType::kAnime, 0, 7}),
	                                     asks({mediaType::kManga, 0, 8})};
	std::sort(sent.begin(), sent.end());
	std::sort(expected.begin(), expected.end());
	CHECK(sent == expected);
}

TEST_CASE("updater fetches a cover before the lookups still waiting") {
	fakeWeb web;
	updater updates(
	    web.fetcher(), web.poster(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>("SIXEL"); },
	    std::chrono::milliseconds(30));
	web.respond(asks(anime(7)), R"({"data":{"Media":{"id":7,
      "coverImage":{"large":"https://img/7.jpg"}}}})");
	web.respond("img/7.jpg", "JPEG");
	updates.lookUp(anime(1));
	updates.lookUp(anime(2));
	updates.lookUp(anime(3));
	CHECK(updates.coverFor(anime(7), 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	REQUIRE(web.posted.size() == 4);
	// The lookups already under way finish; the cover goes before the rest.
	CHECK(web.posted.back() == asks(anime(3)));
	CHECK(web.requested == std::vector<std::string>{"https://img/7.jpg"});
}

TEST_CASE("updater does not hold up quitting while a request is slow") {
	// Shared with the worker, which outlives the updater here.
	auto finished = std::make_shared<std::atomic<bool>>(false);
	auto woke = std::make_shared<std::atomic<bool>>(false);
	auto start = std::chrono::steady_clock::now();
	{
		updater updates([](const std::string&) { return std::optional<std::string>(); },
		                [finished](const std::string&, const std::string&) {
			                std::this_thread::sleep_for(std::chrono::milliseconds(600));
			                *finished = true;
			                return std::optional<std::string>();
		                },
		                catalist::test::testToday,
		                [](const std::string&, int, int) { return std::optional<std::string>(); },
		                std::chrono::milliseconds(0), [woke] { *woke = true; });
		updates.lookUp(anime(1));
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
	// The request ends later on its own, without calling back.
	std::this_thread::sleep_for(std::chrono::milliseconds(800));
	CHECK(finished->load());
	CHECK_FALSE(woke->load());
}
