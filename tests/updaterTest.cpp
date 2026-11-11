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
using catalist::test::fakeWeb;
using catalist::test::makeEntry;

namespace {

const char* kFinishedManga = R"({"data": {"mal_id": 2, "title": "Berserk",
  "chapters": 380, "volumes": 42, "status": "Finished",
  "images": {"jpg": {"image_url": "https://img/berserk.jpg"}}}})";

const char* kAiringAnime = R"({"data": {"mal_id": 9, "title": "Long Runner",
  "episodes": null, "status": "Currently Airing",
  "images": {"jpg": {"image_url": "https://img/runner.jpg"}}}})";

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
	          web.fetcher(), catalist::test::testToday,
	          [this](const std::string& image, int w, int h) { return sixel(image, w, h); },
	          std::chrono::milliseconds(0)) {}

	size_t requestsFor(const std::string& fragment) {
		size_t count = 0;
		for (const auto& url : web.requested)
			count += url.find(fragment) != std::string::npos ? 1 : 0;
		return count;
	}

	fakeWeb web;
	fakeSixel sixel;
	updater updates;
};

}  // namespace

TEST_CASE_FIXTURE(updaterFixture, "updater looks a title up once per run") {
	web.respond("/manga/2", kFinishedManga);
	updates.lookUp(mediaType::kManga, 2);
	updates.lookUp(mediaType::kManga, 2);
	updates.lookUp(mediaType::kManga, 0);
	updates.waitUntilIdle();

	auto findings = updates.takeFindings();
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].type == mediaType::kManga);
	CHECK(findings[0].malId == 2);
	CHECK(findings[0].facts.totalUnits == 380);
	CHECK(findings[0].facts.totalVolumes == 42);
	CHECK(findings[0].facts.released == 380);
	CHECK(web.requested.size() == 1);
	CHECK(updates.takeFindings().empty());
}

TEST_CASE_FIXTURE(updaterFixture, "updater counts the episodes an airing anime has out") {
	web.respond("anime/9/episodes?page=1",
	            R"({"pagination": {"last_visible_page": 2}, "data": []})");
	// The last page: 3 aired by the test date (2026-10-20), 1 still to come.
	web.respond("anime/9/episodes?page=2", R"({"data": [
      {"mal_id": 101, "aired": "2026-10-06T00:00:00+00:00"},
      {"mal_id": 102, "aired": "2026-10-13T00:00:00+00:00"},
      {"mal_id": 103, "aired": null},
      {"mal_id": 104, "aired": "2026-10-27T00:00:00+00:00"}]})");
	web.respond("anime/9", kAiringAnime);
	updates.lookUp(mediaType::kAnime, 9);
	updates.waitUntilIdle();

	auto findings = updates.takeFindings();
	REQUIRE(findings.size() == 1);
	CHECK(findings[0].facts.totalUnits == 0);
	CHECK(findings[0].facts.released == 103);
}

TEST_CASE_FIXTURE(updaterFixture, "updater finds nothing when Jikan cannot be reached") {
	updates.lookUp(mediaType::kAnime, 5);
	updates.waitUntilIdle();
	CHECK(updates.takeFindings().empty());
	CHECK(requestsFor("/anime/5") == 1);
}

TEST_CASE_FIXTURE(updaterFixture, "updater fetches a cover once and converts it per size") {
	web.respond("/manga/2", kFinishedManga);
	web.respond("img/berserk.jpg", "JPEG");
	CHECK(updates.coverFor(mediaType::kManga, 2, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	auto ready = updates.coverFor(mediaType::kManga, 2, 70, 100);
	CHECK(ready.state == updater::coverState::kReady);
	CHECK(ready.sixel == "SIXEL[JPEG 70x100]");

	// Another size converts again, but the image is not fetched again.
	CHECK(updates.coverFor(mediaType::kManga, 2, 140, 200).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(mediaType::kManga, 2, 140, 200).sixel == "SIXEL[JPEG 140x200]");
	CHECK(requestsFor("berserk.jpg") == 1);
	CHECK(requestsFor("/manga/2") == 1);
	CHECK(sixel.calls == 2);
	// Covers never change the catalogue.
	CHECK(updates.takeFindings().empty());
}

TEST_CASE_FIXTURE(updaterFixture, "updater reuses the cover address from a lookup") {
	web.respond("/manga/2", kFinishedManga);
	web.respond("img/berserk.jpg", "JPEG");
	updates.lookUp(mediaType::kManga, 2);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(mediaType::kManga, 2, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(mediaType::kManga, 2, 70, 100).state == updater::coverState::kReady);
	CHECK(requestsFor("/manga/2") == 1);
}

TEST_CASE_FIXTURE(updaterFixture, "updater says why there is no cover") {
	CHECK(updates.coverFor(mediaType::kAnime, 0, 70, 100).state == updater::coverState::kNone);

	// MyAnimeList answers but has no picture.
	web.respond("/anime/8", R"({"data": {"images": {"jpg": {"image_url": null}}}})");
	CHECK(updates.coverFor(mediaType::kAnime, 8, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(mediaType::kAnime, 8, 70, 100).state == updater::coverState::kNone);

	// Offline: unreachable, and not asked for again until the wait is over.
	CHECK(updates.coverFor(mediaType::kAnime, 7, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(mediaType::kAnime, 7, 70, 100).state ==
	      updater::coverState::kUnreachable);
	CHECK(updates.coverFor(mediaType::kAnime, 7, 140, 200).state ==
	      updater::coverState::kUnreachable);
	CHECK(requestsFor("/anime/7") == 1);
}

TEST_CASE("updater tries an unreachable cover again after a while") {
	fakeWeb web;
	updater updates(
	    web.fetcher(), catalist::test::testToday,
	    [](const std::string& image, int, int) { return std::optional<std::string>(image); },
	    std::chrono::milliseconds(0), {}, std::chrono::milliseconds(50));
	CHECK(updates.coverFor(mediaType::kAnime, 7, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(mediaType::kAnime, 7, 70, 100).state ==
	      updater::coverState::kUnreachable);

	// Back online (Jikan was busy, say): the next ask after the wait works.
	web.respond("/anime/7", R"({"data": {"images": {"jpg": {"image_url": "https://img/7.jpg"}}}})");
	web.respond("img/7.jpg", "JPEG");
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
	CHECK(updates.coverFor(mediaType::kAnime, 7, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	auto ready = updates.coverFor(mediaType::kAnime, 7, 70, 100);
	CHECK(ready.state == updater::coverState::kReady);
	CHECK(ready.sixel == "JPEG");
}

TEST_CASE("updater says when the picture cannot be converted") {
	fakeWeb web;
	web.respond("/manga/2", kFinishedManga);
	web.respond("img/berserk.jpg", "not really a JPEG");
	updater updates(
	    web.fetcher(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>(); },
	    std::chrono::milliseconds(0));
	CHECK(updates.coverFor(mediaType::kManga, 2, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	CHECK(updates.coverFor(mediaType::kManga, 2, 70, 100).state ==
	      updater::coverState::kUnreadable);
}

TEST_CASE_FIXTURE(updaterFixture, "updater looks a title up again when Jikan was unreachable") {
	updates.lookUp(mediaType::kManga, 2);
	updates.waitUntilIdle();
	CHECK(updates.takeFindings().empty());
	web.respond("/manga/2", kFinishedManga);
	updates.lookUp(mediaType::kManga, 2);
	updates.waitUntilIdle();
	CHECK(updates.takeFindings().size() == 1);
	CHECK(requestsFor("/manga/2") == 2);
}

TEST_CASE("updater pauses between requests") {
	fakeWeb web;
	updater updates(
	    web.fetcher(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>(); },
	    std::chrono::milliseconds(60));
	auto start = std::chrono::steady_clock::now();
	updates.lookUp(mediaType::kAnime, 1);
	updates.lookUp(mediaType::kAnime, 2);
	updates.lookUp(mediaType::kAnime, 3);
	updates.waitUntilIdle();
	auto elapsed = std::chrono::steady_clock::now() - start;
	CHECK(web.requested.size() == 3);
	// The first request goes at once; each later one waits its turn.
	CHECK(elapsed >= std::chrono::milliseconds(120));
}

TEST_CASE_FIXTURE(catalist::test::tempDirFixture,
                  "checkFollowed asks only for followed titles not checked today") {
	catalist::catalogue lists(tempDir());
	lists.addEntry(makeEntry().malId(1).status(watchStatus::kWatching));
	lists.addEntry(makeEntry().malId(2).status(watchStatus::kOnHold));
	lists.addEntry(makeEntry().malId(3).status(watchStatus::kCompleted));
	lists.addEntry(makeEntry().malId(4).status(watchStatus::kPlanToWatch));
	lists.addEntry(makeEntry().status(watchStatus::kWatching));
	lists.addEntry(makeEntry().malId(6).status(watchStatus::kWatching).checkedAt("2026-10-20"));
	lists.addEntry(makeEntry().malId(7).status(watchStatus::kWatching).checkedAt("2026-10-19"));
	lists.addEntry(makeEntry().malId(8).type(mediaType::kManga).status(watchStatus::kWatching));

	fakeWeb web;
	updater updates(
	    web.fetcher(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>(); },
	    std::chrono::milliseconds(0));
	catalist::checkFollowed(lists, updates, catalist::test::testToday());
	updates.waitUntilIdle();
	std::vector<std::string> asked = web.requested;
	std::sort(asked.begin(), asked.end());
	CHECK(asked == std::vector<std::string>{
	                   "https://api.jikan.moe/v4/anime/1", "https://api.jikan.moe/v4/anime/2",
	                   "https://api.jikan.moe/v4/anime/7", "https://api.jikan.moe/v4/manga/8"});
}

TEST_CASE("updater fetches a cover before the lookups still waiting") {
	fakeWeb web;
	updater updates(
	    web.fetcher(), catalist::test::testToday,
	    [](const std::string&, int, int) { return std::optional<std::string>("SIXEL"); },
	    std::chrono::milliseconds(30));
	web.respond("/anime/7", R"({"data": {"images": {"jpg": {"image_url":
      "https://img/7.jpg"}}}})");
	web.respond("img/7.jpg", "JPEG");
	updates.lookUp(mediaType::kAnime, 1);
	updates.lookUp(mediaType::kAnime, 2);
	updates.lookUp(mediaType::kAnime, 3);
	CHECK(updates.coverFor(mediaType::kAnime, 7, 70, 100).state == updater::coverState::kLoading);
	updates.waitUntilIdle();
	REQUIRE(web.requested.size() == 5);
	// The lookups already under way finish; the cover goes before the rest.
	CHECK(web.requested.back() == "https://api.jikan.moe/v4/anime/3");
}

TEST_CASE("updater does not hold up quitting while a request is slow") {
	// Shared with the worker, which outlives the updater here.
	auto finished = std::make_shared<std::atomic<bool>>(false);
	auto woke = std::make_shared<std::atomic<bool>>(false);
	auto start = std::chrono::steady_clock::now();
	{
		updater updates(
		    [finished](const std::string&) {
			    std::this_thread::sleep_for(std::chrono::milliseconds(600));
			    *finished = true;
			    return std::optional<std::string>();
		    },
		    catalist::test::testToday,
		    [](const std::string&, int, int) { return std::optional<std::string>(); },
		    std::chrono::milliseconds(0), [woke] { *woke = true; });
		updates.lookUp(mediaType::kAnime, 1);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
	// The request ends later on its own, without calling back.
	std::this_thread::sleep_for(std::chrono::milliseconds(800));
	CHECK(finished->load());
	CHECK_FALSE(woke->load());
}
