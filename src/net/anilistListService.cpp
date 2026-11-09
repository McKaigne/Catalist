#include "catalist/net/anilistListService.hpp"

#include <cmath>
#include <set>

namespace catalist {

namespace {

constexpr const char* kEndpoint = "https://graphql.anilist.co";
constexpr int kChunkSize = 500;
constexpr size_t kLookupPageSize = 50;

const char* kViewerQuery = "query { Viewer { id name } }";

const char* kListQuery = R"(query ($userId: Int, $userName: String, $type: MediaType, $chunk: Int, $perChunk: Int) {
  MediaListCollection(userId: $userId, userName: $userName, type: $type, chunk: $chunk, perChunk: $perChunk, forceSingleCompletedList: true) {
    hasNextChunk
    lists {
      isCustomList
      entries {
        id status score(format: POINT_10) progress progressVolumes repeat notes updatedAt
        startedAt { year month day }
        completedAt { year month day }
        media { id idMal episodes chapters volumes title { romaji english } }
      }
    }
  }
})";

const char* kSaveMutation = R"(mutation ($mediaId: Int, $status: MediaListStatus, $scoreRaw: Int, $progress: Int, $progressVolumes: Int, $repeat: Int, $notes: String, $startedAt: FuzzyDateInput, $completedAt: FuzzyDateInput) {
  SaveMediaListEntry(mediaId: $mediaId, status: $status, scoreRaw: $scoreRaw, progress: $progress, progressVolumes: $progressVolumes, repeat: $repeat, notes: $notes, startedAt: $startedAt, completedAt: $completedAt) { id }
})";

const char* kDeleteMutation = R"(mutation ($id: Int) {
  DeleteMediaListEntry(id: $id) { deleted }
})";

const char* kLookupQuery = R"(query ($malIds: [Int], $ids: [Int], $type: MediaType) {
  Page(perPage: 50) { media(idMal_in: $malIds, id_in: $ids, type: $type) { id idMal } }
})";

std::string typeName(mediaType type) {
  return type == mediaType::kAnime ? "ANIME" : "MANGA";
}

int intField(const json::value& item, const char* key) {
  return static_cast<int>(item[key].asInteger().value_or(0));
}

watchStatus statusFromAnilist(const std::string& status) {
  if (status == "COMPLETED")
    return watchStatus::kCompleted;
  if (status == "PAUSED")
    return watchStatus::kOnHold;
  if (status == "DROPPED")
    return watchStatus::kDropped;
  if (status == "PLANNING")
    return watchStatus::kPlanToWatch;
  return watchStatus::kWatching;
}

std::string statusToAnilist(watchStatus status) {
  switch (status) {
    case watchStatus::kWatching: return "CURRENT";
    case watchStatus::kCompleted: return "COMPLETED";
    case watchStatus::kOnHold: return "PAUSED";
    case watchStatus::kDropped: return "DROPPED";
    case watchStatus::kPlanToWatch: return "PLANNING";
  }
  return "CURRENT";
}

std::optional<isoDate> fuzzyDate(const json::value& date) {
  auto year = date["year"].asInteger();
  auto month = date["month"].asInteger();
  auto day = date["day"].asInteger();
  if (!year || !month || !day)
    return std::nullopt;
  return isoDate::fromParts(static_cast<int>(*year), static_cast<int>(*month),
                            static_cast<int>(*day));
}

json::value fuzzyDateInput(const isoDate& date) {
  json::value input;
  input.set("year", date.year());
  input.set("month", date.month());
  input.set("day", date.day());
  return input;
}

std::optional<isoDate> dateFromUnixSeconds(const json::value& seconds) {
  auto value = seconds.asInteger();
  if (!value || *value <= 0)
    return std::nullopt;
  return isoDate::fromDayNumber(static_cast<long>(*value / 86400));
}

std::string titleOf(const json::value& media) {
  auto romaji = media["title"]["romaji"].asString();
  if (romaji && !romaji->empty())
    return *romaji;
  return media["title"]["english"].asString().value_or("");
}

remoteListEntry rowFromJson(const json::value& row, mediaType type) {
  const json::value& media = row["media"];
  bool anime = type == mediaType::kAnime;
  remoteListEntry converted;
  converted.listEntryId = row["id"].asInteger();
  entry& item = converted.item;
  item.type = type;
  item.title = titleOf(media);
  item.setRemoteId("anilist", std::to_string(intField(media, "id")));
  if (int malId = intField(media, "idMal"); malId > 0)
    item.setRemoteId("mal", std::to_string(malId));
  item.totalUnits = intField(media, anime ? "episodes" : "chapters");
  item.totalVolumes = anime ? 0 : intField(media, "volumes");
  item.status = statusFromAnilist(row["status"].asString().value_or(""));
  item.score = static_cast<int>(std::lround(row["score"].asNumber().value_or(0)));
  item.progress = intField(row, "progress");
  item.volumes = anime ? 0 : intField(row, "progressVolumes");
  item.rewatched = intField(row, "repeat");
  item.notes = row["notes"].asString().value_or("");
  item.startDate = fuzzyDate(row["startedAt"]);
  item.finishDate = fuzzyDate(row["completedAt"]);
  item.updatedAt = dateFromUnixSeconds(row["updatedAt"]);
  return converted;
}

// Appends the rows of one chunk, skipping custom lists (they repeat entries
// already in the status lists) and anything seen before.
void appendChunk(const json::value& collection, mediaType type,
                 std::set<long long>& seen,
                 std::vector<remoteListEntry>& rows) {
  for (const auto& group : collection["lists"].items()) {
    if (group["isCustomList"].asBool().value_or(false))
      continue;
    for (const auto& row : group["entries"].items()) {
      auto rowId = row["id"].asInteger();
      if (!rowId || !seen.insert(*rowId).second)
        continue;
      rows.push_back(rowFromJson(row, type));
    }
  }
}

json::value intArray(const std::vector<int>& ids, size_t from, size_t count) {
  json::value list;
  for (size_t i = from; i < ids.size() && i < from + count; ++i)
    list.push(ids[i]);
  return list;
}

std::string firstError(const json::value& reply) {
  const auto& errors = reply["errors"].items();
  if (errors.empty())
    return "";
  return errors.front()["message"].asString().value_or("unknown error");
}

}  // namespace

anilistListService::anilistListService(iHttpClient& http, anilistAuth& auth,
                                       std::function<void()> pace)
    : http_(http), auth_(auth), pace_(std::move(pace)) {}

bool anilistListService::isConnected() const {
  return auth_.isAuthenticated();
}

std::optional<int> anilistListService::idOf(const entry& item) const {
  if (item.anilistId > 0)
    return item.anilistId;
  return std::nullopt;
}

result<json::value> anilistListService::query(const std::string& document,
                                              json::value variables) {
  using queryResult = result<json::value>;
  json::value payload;
  payload.set("query", document);
  payload.set("variables", std::move(variables));
  httpRequest request;
  request.url = kEndpoint;
  request.method = "POST";
  request.headers["Content-Type"] = "application/json";
  request.headers["Accept"] = "application/json";
  std::string token = auth_.accessToken();
  if (!token.empty())
    request.headers["Authorization"] = "Bearer " + token;
  request.body = json::serialize(payload);
  if (pace_)
    pace_();

  auto response = http_.execute(request);
  if (!response.ok())
    return queryResult::failure(response.errorMessage());
  if (response.value().statusCode == 429)
    return queryResult::failure("AniList rate limit reached; try again in a minute.");
  auto reply = json::parse(response.value().body);
  if (!reply.ok())
    return queryResult::failure("Unreadable AniList reply (HTTP " +
                                std::to_string(response.value().statusCode) + ").");
  std::string error = firstError(reply.value());
  if (!error.empty())
    return queryResult::failure("AniList: " + error);
  if (response.value().statusCode != 200)
    return queryResult::failure("AniList answered HTTP " +
                                std::to_string(response.value().statusCode) + ".");
  return queryResult::success(reply.value()["data"]);
}

result<anilistViewer> anilistListService::viewer() {
  auto data = query(kViewerQuery, json::value::object{});
  if (!data.ok())
    return result<anilistViewer>::failure(data.errorMessage());
  const json::value& found = data.value()["Viewer"];
  auto id = found["id"].asInteger();
  if (!id)
    return result<anilistViewer>::failure("AniList did not accept the token.");
  return result<anilistViewer>::success(
      {static_cast<int>(*id), found["name"].asString().value_or("")});
}

result<std::vector<remoteListEntry>> anilistListService::fetchList(
    mediaType type, const std::optional<std::string>& userName) {
  using listResult = result<std::vector<remoteListEntry>>;
  json::value variables;
  variables.set("type", typeName(type));
  variables.set("perChunk", kChunkSize);
  if (userName) {
    variables.set("userName", *userName);
  } else {
    auto loggedIn = viewer();
    if (!loggedIn.ok())
      return listResult::failure(loggedIn.errorMessage());
    variables.set("userId", loggedIn.value().id);
  }

  std::vector<remoteListEntry> rows;
  std::set<long long> seen;
  for (int chunk = 1;; ++chunk) {
    variables.set("chunk", chunk);
    auto data = query(kListQuery, variables);
    if (!data.ok())
      return listResult::failure(data.errorMessage());
    const json::value& collection = data.value()["MediaListCollection"];
    appendChunk(collection, type, seen, rows);
    if (!collection["hasNextChunk"].asBool().value_or(false))
      return listResult::success(std::move(rows));
  }
}

syncOutcome anilistListService::saveEntry(const entry& item) {
  auto id = idOf(item);
  if (!id)
    return syncOutcome::failure("'" + item.title + "' has no AniList id.");
  json::value variables;
  variables.set("mediaId", *id);
  variables.set("status", statusToAnilist(item.status));
  variables.set("scoreRaw", item.score * 10);
  variables.set("progress", item.progress);
  if (item.type == mediaType::kManga)
    variables.set("progressVolumes", item.volumes);
  variables.set("repeat", item.rewatched);
  variables.set("notes", item.notes);
  if (item.startDate)
    variables.set("startedAt", fuzzyDateInput(*item.startDate));
  if (item.finishDate)
    variables.set("completedAt", fuzzyDateInput(*item.finishDate));
  auto data = query(kSaveMutation, std::move(variables));
  if (!data.ok())
    return syncOutcome::failure("Saving '" + item.title + "': " + data.errorMessage());
  return syncSucceeded();
}

syncOutcome anilistListService::deleteEntry(const remoteListEntry& row) {
  if (!row.listEntryId)
    return syncOutcome::failure("'" + row.item.title + "' has no AniList list row.");
  json::value variables;
  variables.set("id", *row.listEntryId);
  auto data = query(kDeleteMutation, std::move(variables));
  if (!data.ok())
    return syncOutcome::failure("Deleting '" + row.item.title + "': " +
                                data.errorMessage());
  return syncSucceeded();
}

result<std::map<int, int>> anilistListService::lookup(
    mediaType type, const std::vector<int>& ids, bool idsAreMal) {
  using mapResult = result<std::map<int, int>>;
  std::map<int, int> found;
  for (size_t from = 0; from < ids.size(); from += kLookupPageSize) {
    json::value variables;
    variables.set("type", typeName(type));
    variables.set(idsAreMal ? "malIds" : "ids", intArray(ids, from, kLookupPageSize));
    auto data = query(kLookupQuery, std::move(variables));
    if (!data.ok())
      return mapResult::failure(data.errorMessage());
    for (const auto& media : data.value()["Page"]["media"].items()) {
      int anilistId = intField(media, "id");
      int malId = intField(media, "idMal");
      if (anilistId > 0 && malId > 0)
        found[idsAreMal ? malId : anilistId] = idsAreMal ? anilistId : malId;
    }
  }
  return mapResult::success(std::move(found));
}

result<std::map<int, int>> anilistListService::anilistIdsForMal(
    mediaType type, const std::vector<int>& malIds) {
  return lookup(type, malIds, true);
}

result<std::map<int, int>> anilistListService::malIdsForAnilist(
    mediaType type, const std::vector<int>& anilistIds) {
  return lookup(type, anilistIds, false);
}

}  // namespace catalist
