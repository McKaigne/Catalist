#include <doctest/doctest.h>

#include <string>

#include "catalist/core/json.hpp"
#include "catalist/net/anilistAuth.hpp"
#include "catalist/net/anilistListService.hpp"
#include "catalist/net/malAuth.hpp"
#include "catalist/net/malListService.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "support/testSupport.hpp"

using catalist::httpRequest;
using catalist::httpResponse;
using catalist::mediaType;
using catalist::watchStatus;

namespace {

class silentAuthServer : public catalist::iAuthCallbackServer {
public:
  std::optional<std::string> waitForCode(std::chrono::seconds) override {
    return std::nullopt;
  }
};

bool seedMalLogin(catalist::memoryFileSystem& fs) {
  return fs.writeFileAtomic(
      "/data/mal_auth.json",
      R"({"access_token": "tok", "refresh_token": "ref", "expires_at": 0})");
}

// Logged in to MAL with a token that never expires.
struct malFixture {
  catalist::memoryFileSystem fs;
  bool seeded = seedMalLogin(fs);
  catalist::mockHttpClient http;
  catalist::fakeProcessLauncher launcher;
  silentAuthServer server;
  catalist::fixedClock clock = catalist::test::makeTestClock();
  catalist::malAuth auth{fs, http, launcher, server, clock, "/data/mal_auth.json"};
  int paced = 0;
  catalist::malListService service{http, auth, [this] { ++paced; }};
};

const char* kMalAnimePage1 = R"({
  "data": [
    {"node": {"id": 52991, "title": "Sousou no Frieren", "num_episodes": 28},
     "list_status": {"status": "completed", "score": 10, "num_episodes_watched": 28,
                     "num_times_rewatched": 1, "start_date": "2023-09-29",
                     "finish_date": "2024-03", "comments": "Great",
                     "updated_at": "2024-03-22T10:00:00+00:00"}}
  ],
  "paging": {"next": "https://api.myanimelist.net/v2/users/@me/animelist?offset=1000"}
})";

const char* kMalAnimePage2 = R"({
  "data": [
    {"node": {"id": 44265, "title": "Odd Taxi", "num_episodes": 13},
     "list_status": {"status": "plan_to_watch", "score": 0, "num_episodes_watched": 0}}
  ],
  "paging": {}
})";

std::string formValue(const std::string& body, const std::string& key) {
  std::string marker = key + "=";
  size_t start = body.find(marker);
  if (start == std::string::npos)
    return "<missing>";
  start += marker.size();
  return body.substr(start, body.find('&', start) - start);
}

}  // namespace

TEST_CASE_FIXTURE(malFixture, "mal list service reads every page with the bearer token") {
  http.setResponse("users/@me/animelist?limit", {200, kMalAnimePage1, ""});
  http.setResponse("offset=1000", {200, kMalAnimePage2, ""});
  auto rows = service.fetchList(mediaType::kAnime, std::nullopt);
  REQUIRE(rows.ok());
  REQUIRE(rows.value().size() == 2);
  const auto& frieren = rows.value()[0].item;
  CHECK(frieren.malId == 52991);
  CHECK(frieren.title == "Sousou no Frieren");
  CHECK(frieren.status == watchStatus::kCompleted);
  CHECK(frieren.score == 10);
  CHECK(frieren.progress == 28);
  CHECK(frieren.totalUnits == 28);
  CHECK(frieren.rewatched == 1);
  CHECK(frieren.startDate->toString() == "2023-09-29");
  CHECK_FALSE(frieren.finishDate.has_value());
  CHECK(frieren.notes == "Great");
  CHECK(frieren.updatedAt->toString() == "2024-03-22");
  CHECK(rows.value()[1].item.status == watchStatus::kPlanToWatch);
  REQUIRE(http.requests().size() == 2);
  CHECK(http.requests()[0].headers.at("Authorization") == "Bearer tok");
  CHECK(http.requests()[0].url.find("fields=list_status%7b") != std::string::npos);
  CHECK(paced == 2);
}

TEST_CASE_FIXTURE(malFixture, "mal list service never follows paging off the API host") {
  http.setResponse("users/@me/animelist", {200,
      R"({"data": [], "paging": {"next": "https://evil.example/steal"}})", ""});
  auto rows = service.fetchList(mediaType::kAnime, std::nullopt);
  REQUIRE(rows.ok());
  CHECK(http.requests().size() == 1);
}

TEST_CASE_FIXTURE(malFixture, "mal list service reads a public list with the client id only") {
  http.setResponse("users/some%20one/mangalist", {200,
      R"({"data": [{"node": {"id": 2, "title": "Berserk", "num_chapters": 0, "num_volumes": 0},
                    "list_status": {"status": "reading", "score": 10, "num_chapters_read": 364,
                                    "num_volumes_read": 42}}]})", ""});
  auto rows = service.fetchList(mediaType::kManga, std::string("some one"));
  REQUIRE(rows.ok());
  REQUIRE(rows.value().size() == 1);
  CHECK(rows.value()[0].item.progress == 364);
  CHECK(rows.value()[0].item.volumes == 42);
  CHECK(rows.value()[0].item.status == watchStatus::kWatching);
  CHECK(http.requests()[0].headers.count("Authorization") == 0);
  CHECK(http.requests()[0].headers.at("X-MAL-CLIENT-ID") == catalist::kMalClientId);
}

TEST_CASE_FIXTURE(malFixture, "mal list service reports API errors") {
  http.setResponse("users/@me/animelist", {401, R"({"error": "invalid_token"})", ""});
  auto rows = service.fetchList(mediaType::kAnime, std::nullopt);
  REQUIRE_FALSE(rows.ok());
  CHECK(rows.errorMessage() == "MyAnimeList answered HTTP 401 (invalid_token)");
}

TEST_CASE_FIXTURE(malFixture, "mal list service saves a manga row as an encoded form") {
  http.setResponse("manga/2/my_list_status", {200, "{}", ""});
  catalist::entry berserk = catalist::test::makeEntry()
                                .id(2).malId(2).type(mediaType::kManga)
                                .title("Berserk").status(watchStatus::kPlanToWatch)
                                .score(9).progress(10).volumes(1)
                                .notes("a&b=c");
  REQUIRE(service.saveEntry(berserk).ok());
  const auto& sent = http.requests().back();
  CHECK(sent.method == "PATCH");
  CHECK(sent.url == "https://api.myanimelist.net/v2/manga/2/my_list_status");
  CHECK(formValue(sent.body, "status") == "plan_to_read");
  CHECK(formValue(sent.body, "score") == "9");
  CHECK(formValue(sent.body, "num_chapters_read") == "10");
  CHECK(formValue(sent.body, "num_volumes_read") == "1");
  CHECK(formValue(sent.body, "comments") == "a%26b%3dc");
  CHECK(formValue(sent.body, "start_date") == "<missing>");
}

TEST_CASE_FIXTURE(malFixture, "mal list service treats deleting a missing row as done") {
  http.setResponse("anime/5/my_list_status", {404, "{}", ""});
  catalist::remoteListEntry row;
  row.item = catalist::test::makeEntry().id(5).malId(5).title("Gone");
  REQUIRE(service.deleteEntry(row).ok());
  CHECK(http.requests().back().method == "DELETE");
}

TEST_CASE("mal list service needs a login to change the list") {
  catalist::memoryFileSystem fs;
  catalist::mockHttpClient http;
  catalist::fakeProcessLauncher launcher;
  silentAuthServer server;
  auto clock = catalist::test::makeTestClock();
  catalist::malAuth auth(fs, http, launcher, server, clock, "/data/mal_auth.json");
  catalist::malListService service(http, auth, nullptr);
  CHECK_FALSE(service.isConnected());
  auto saved = service.saveEntry(catalist::test::makeEntry().id(1).malId(1).title("A"));
  REQUIRE_FALSE(saved.ok());
  CHECK(http.requests().empty());
}

TEST_CASE("malAuth refreshes an expiring token with the refresh token") {
  catalist::memoryFileSystem fs;
  catalist::mockHttpClient http;
  catalist::fakeProcessLauncher launcher;
  silentAuthServer server;
  auto clock = catalist::test::makeTestClock();
  (void)fs.writeFileAtomic("/data/mal_auth.json",
                           R"({"access_token": "old", "refresh_token": "ref", "expires_at": 10})");
  http.setResponse("oauth2/token", {200,
      R"({"access_token": "new", "refresh_token": "ref2", "expires_in": 2678400})", ""});
  catalist::malAuth auth(fs, http, launcher, server, clock, "/data/mal_auth.json");
  REQUIRE(auth.ensureFreshToken());
  CHECK(auth.getAccessToken() == "new");
  CHECK(formValue(http.requests()[0].body, "grant_type") == "refresh_token");
  CHECK(formValue(http.requests()[0].body, "refresh_token") == "ref");
  REQUIRE(auth.ensureFreshToken());
  CHECK(http.requests().size() == 1);
}

namespace {

struct anilistFixture {
  catalist::memoryFileSystem fs;
  catalist::mockHttpClient http;
  catalist::anilistAuth auth{fs, "/data/anilist_auth.json", "1234"};
  catalist::anilistListService service{http, auth, nullptr};
  std::vector<catalist::json::value> sentBodies() const {
    std::vector<catalist::json::value> bodies;
    for (const auto& request : http.requests())
      bodies.push_back(catalist::json::parse(request.body).value());
    return bodies;
  }
};

std::optional<httpResponse> answerByQuery(const httpRequest& request) {
  auto body = catalist::json::parse(request.body);
  if (!body.ok())
    return std::nullopt;
  std::string document = body.value()["query"].asString().value_or("");
  if (document.find("Viewer") != std::string::npos)
    return httpResponse{200, R"({"data": {"Viewer": {"id": 77, "name": "me"}}})", ""};
  if (document.find("MediaListCollection") == std::string::npos)
    return std::nullopt;
  auto chunk = body.value()["variables"]["chunk"].asInteger().value_or(0);
  if (chunk == 1)
    return httpResponse{200, R"({"data": {"MediaListCollection": {"hasNextChunk": true, "lists": [
      {"isCustomList": false, "entries": [
        {"id": 900, "status": "REPEATING", "score": 8.6, "progress": 3, "progressVolumes": 0,
         "repeat": 2, "notes": "again", "updatedAt": 1729425600,
         "startedAt": {"year": 2024, "month": 1, "day": 5},
         "completedAt": {"year": null, "month": null, "day": null},
         "media": {"id": 154587, "idMal": 52991, "episodes": 28, "chapters": null, "volumes": null,
                   "title": {"romaji": "Sousou no Frieren", "english": "Frieren"}}}]},
      {"isCustomList": true, "entries": [
        {"id": 900, "status": "REPEATING", "media": {"id": 154587}}]}]}}})", ""};
  return httpResponse{200, R"({"data": {"MediaListCollection": {"hasNextChunk": false, "lists": [
      {"isCustomList": false, "entries": [
        {"id": 901, "status": "PAUSED", "score": 0, "progress": 1,
         "media": {"id": 1, "idMal": null, "episodes": 26,
                   "title": {"romaji": "", "english": "Cowboy Bebop"}}}]}]}}})", ""};
}

}  // namespace

TEST_CASE("anilist auth builds the PIN login URL and stores the pasted token privately") {
  catalist::memoryFileSystem fs;
  catalist::anilistAuth auth(fs, "/data/anilist_auth.json", "1234");
  CHECK(auth.authorizeUrl() ==
        "https://anilist.co/api/v2/oauth/authorize?client_id=1234&response_type=token");
  CHECK_FALSE(auth.storeToken("  \n"));
  REQUIRE(auth.storeToken("  secret-token\n"));
  catalist::anilistAuth reloaded(fs, "/data/anilist_auth.json", "1234");
  CHECK(reloaded.accessToken() == "secret-token");
  reloaded.logout();
  CHECK_FALSE(reloaded.isAuthenticated());
  CHECK_FALSE(fs.exists("/data/anilist_auth.json"));

  catalist::anilistAuth noClient(fs, "/data/anilist_auth.json", "");
  CHECK_FALSE(noClient.authorizeUrl().has_value());
}

TEST_CASE_FIXTURE(anilistFixture, "anilist list service reads every chunk once and maps fields") {
  REQUIRE(auth.storeToken("tok"));
  http.setResponder(answerByQuery);
  auto rows = service.fetchList(mediaType::kAnime, std::nullopt);
  REQUIRE(rows.ok());
  REQUIRE(rows.value().size() == 2);
  const auto& frieren = rows.value()[0];
  CHECK(frieren.listEntryId == 900);
  CHECK(frieren.item.anilistId == 154587);
  CHECK(frieren.item.malId == 52991);
  CHECK(frieren.item.title == "Sousou no Frieren");
  CHECK(frieren.item.status == watchStatus::kWatching);
  CHECK(frieren.item.score == 9);
  CHECK(frieren.item.rewatched == 2);
  CHECK(frieren.item.notes == "again");
  CHECK(frieren.item.startDate->toString() == "2024-01-05");
  CHECK_FALSE(frieren.item.finishDate.has_value());
  CHECK(frieren.item.updatedAt->toString() == "2024-10-20");
  const auto& bebop = rows.value()[1].item;
  CHECK(bebop.title == "Cowboy Bebop");
  CHECK(bebop.malId == 0);
  CHECK(bebop.status == watchStatus::kOnHold);

  auto bodies = sentBodies();
  REQUIRE(bodies.size() == 3);
  CHECK(bodies[1]["variables"]["userId"].asInteger() == 77);
  CHECK(bodies[1]["variables"]["type"].asString() == "ANIME");
  CHECK(http.requests()[1].headers.at("Authorization") == "Bearer tok");
}

TEST_CASE_FIXTURE(anilistFixture, "anilist list service reads a public list by name without a token") {
  http.setResponder(answerByQuery);
  auto rows = service.fetchList(mediaType::kAnime, std::string("someone"));
  REQUIRE(rows.ok());
  auto bodies = sentBodies();
  CHECK(bodies[0]["variables"]["userName"].asString() == "someone");
  CHECK(http.requests()[0].headers.count("Authorization") == 0);
}

TEST_CASE_FIXTURE(anilistFixture, "anilist list service saves with a raw score and fuzzy dates") {
  REQUIRE(auth.storeToken("tok"));
  http.setResponse("graphql.anilist.co", {200, R"({"data": {"SaveMediaListEntry": {"id": 5}}})", ""});
  catalist::entry berserk = catalist::test::makeEntry()
                                .id(2).anilistId(30002).type(mediaType::kManga)
                                .title("Berserk").status(watchStatus::kOnHold)
                                .score(7).progress(364).volumes(42)
                                .startDate("2019-05-01");
  REQUIRE(service.saveEntry(berserk).ok());
  auto variables = sentBodies()[0]["variables"];
  CHECK(variables["mediaId"].asInteger() == 30002);
  CHECK(variables["status"].asString() == "PAUSED");
  CHECK(variables["scoreRaw"].asInteger() == 70);
  CHECK(variables["progressVolumes"].asInteger() == 42);
  CHECK(variables["startedAt"]["month"].asInteger() == 5);
  CHECK(variables["completedAt"].isNull());
}

TEST_CASE_FIXTURE(anilistFixture, "anilist list service surfaces GraphQL errors") {
  REQUIRE(auth.storeToken("bad"));
  http.setResponse("graphql.anilist.co",
                   {400, R"({"errors": [{"message": "Invalid token"}], "data": null})", ""});
  auto result = service.viewer();
  REQUIRE_FALSE(result.ok());
  CHECK(result.errorMessage() == "AniList: Invalid token");
}

TEST_CASE_FIXTURE(anilistFixture, "anilist id lookup asks in pages of fifty") {
  http.setResponder([](const httpRequest& request) -> std::optional<httpResponse> {
    auto ids = catalist::json::parse(request.body).value()["variables"]["malIds"].items();
    std::string media;
    for (const auto& id : ids) {
      long long mal = id.asInteger().value_or(0);
      media += (media.empty() ? "" : ",") + std::string("{\"id\": ") +
               std::to_string(mal + 100000) + ", \"idMal\": " + std::to_string(mal) + "}";
    }
    return httpResponse{200, R"({"data": {"Page": {"media": [)" + media + "]}}}", ""};
  });
  std::vector<int> malIds;
  for (int id = 1; id <= 120; ++id)
    malIds.push_back(id);
  auto mapped = service.anilistIdsForMal(mediaType::kAnime, malIds);
  REQUIRE(mapped.ok());
  CHECK(mapped.value().size() == 120);
  CHECK(mapped.value().at(7) == 100007);
  CHECK(http.requests().size() == 3);
}
