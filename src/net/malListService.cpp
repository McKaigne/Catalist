#include "catalist/net/malListService.hpp"

#include "catalist/core/json.hpp"
#include "catalist/core/url.hpp"

namespace catalist {

namespace {

constexpr const char* kApiRoot = "https://api.myanimelist.net/v2/";

const char* kAnimeFields =
    "list_status{status,score,num_episodes_watched,num_times_rewatched,"
    "start_date,finish_date,comments,updated_at},num_episodes";
const char* kMangaFields =
    "list_status{status,score,num_chapters_read,num_volumes_read,"
    "num_times_reread,start_date,finish_date,comments,updated_at},"
    "num_chapters,num_volumes";

std::string kindPath(mediaType type) {
  return type == mediaType::kAnime ? "anime" : "manga";
}

int intField(const json::value& item, const char* key) {
  return static_cast<int>(item[key].asInteger().value_or(0));
}

watchStatus statusFromMal(const std::string& status) {
  if (status == "completed")
    return watchStatus::kCompleted;
  if (status == "on_hold")
    return watchStatus::kOnHold;
  if (status == "dropped")
    return watchStatus::kDropped;
  if (status == "plan_to_watch" || status == "plan_to_read")
    return watchStatus::kPlanToWatch;
  return watchStatus::kWatching;
}

std::string statusToMal(watchStatus status, mediaType type) {
  bool anime = type == mediaType::kAnime;
  switch (status) {
    case watchStatus::kWatching: return anime ? "watching" : "reading";
    case watchStatus::kCompleted: return "completed";
    case watchStatus::kOnHold: return "on_hold";
    case watchStatus::kDropped: return "dropped";
    case watchStatus::kPlanToWatch: return anime ? "plan_to_watch" : "plan_to_read";
  }
  return "watching";
}

std::optional<isoDate> dateField(const json::value& item, const char* key) {
  auto text = item[key].asString();
  return text ? isoDate::parse(*text) : std::nullopt;
}

remoteListEntry rowFromJson(const json::value& data, mediaType type) {
  const json::value& node = data["node"];
  const json::value& status = data["list_status"];
  bool anime = type == mediaType::kAnime;
  remoteListEntry row;
  entry& item = row.item;
  item.type = type;
  item.title = node["title"].asString().value_or("");
  item.malId = intField(node, "id");
  item.totalUnits = intField(node, anime ? "num_episodes" : "num_chapters");
  item.totalVolumes = anime ? 0 : intField(node, "num_volumes");
  item.status = statusFromMal(status["status"].asString().value_or(""));
  item.score = intField(status, "score");
  item.progress =
      intField(status, anime ? "num_episodes_watched" : "num_chapters_read");
  item.volumes = anime ? 0 : intField(status, "num_volumes_read");
  item.rewatched =
      intField(status, anime ? "num_times_rewatched" : "num_times_reread");
  item.startDate = dateField(status, "start_date");
  item.finishDate = dateField(status, "finish_date");
  item.notes = status["comments"].asString().value_or("");
  item.updatedAt = dateField(status, "updated_at");
  return row;
}

std::string apiError(const httpResponse& response) {
  auto parsed = json::parse(response.body);
  std::string detail =
      parsed.ok() ? parsed.value()["error"].asString().value_or("") : "";
  return "MyAnimeList answered HTTP " + std::to_string(response.statusCode) +
         (detail.empty() ? "" : " (" + detail + ")");
}

std::string listStatusForm(const entry& item) {
  bool anime = item.type == mediaType::kAnime;
  std::string form = "status=" + statusToMal(item.status, item.type) +
                     "&score=" + std::to_string(item.score);
  if (anime) {
    form += "&num_watched_episodes=" + std::to_string(item.progress) +
            "&num_times_rewatched=" + std::to_string(item.rewatched);
  } else {
    form += "&num_chapters_read=" + std::to_string(item.progress) +
            "&num_volumes_read=" + std::to_string(item.volumes) +
            "&num_times_reread=" + std::to_string(item.rewatched);
  }
  if (item.startDate)
    form += "&start_date=" + item.startDate->toString();
  if (item.finishDate)
    form += "&finish_date=" + item.finishDate->toString();
  return form + "&comments=" + url::encode(item.notes);
}

// paging.next must stay on the API host, or the bearer token would be sent
// wherever a response pointed.
bool isApiUrl(const std::string& next) {
  return next.rfind(kApiRoot, 0) == 0;
}

}  // namespace

malListService::malListService(iHttpClient& http, malAuth& auth,
                               std::function<void()> pace)
    : http_(http), auth_(auth), pace_(std::move(pace)) {}

bool malListService::isConnected() const {
  return auth_.isAuthenticated();
}

std::optional<int> malListService::idOf(const entry& item) const {
  if (item.malId > 0)
    return item.malId;
  return std::nullopt;
}

result<httpResponse> malListService::send(httpRequest request,
                                          bool needsLogin) {
  if (needsLogin && !auth_.ensureFreshToken())
    return result<httpResponse>::failure(
        "Not logged in to MyAnimeList (or the login expired).");
  if (needsLogin)
    request.headers["Authorization"] = "Bearer " + auth_.getAccessToken();
  else
    request.headers["X-MAL-CLIENT-ID"] = kMalClientId;
  if (pace_)
    pace_();
  return http_.execute(request);
}

result<std::vector<remoteListEntry>> malListService::fetchList(
    mediaType type, const std::optional<std::string>& userName) {
  using listResult = result<std::vector<remoteListEntry>>;
  std::string user = userName ? url::encode(*userName) : "@me";
  httpRequest request;
  request.url = std::string(kApiRoot) + "users/" + user + "/" + kindPath(type) +
                "list?limit=1000&nsfw=true&fields=" +
                url::encode(type == mediaType::kAnime ? kAnimeFields
                                                      : kMangaFields);
  std::vector<remoteListEntry> rows;
  while (true) {
    auto response = send(request, !userName.has_value());
    if (!response.ok())
      return listResult::failure(response.errorMessage());
    if (response.value().statusCode != 200)
      return listResult::failure(apiError(response.value()));
    auto page = json::parse(response.value().body);
    if (!page.ok())
      return listResult::failure("Unreadable MyAnimeList reply: " +
                                 page.errorMessage());
    for (const auto& data : page.value()["data"].items())
      rows.push_back(rowFromJson(data, type));
    auto next = page.value()["paging"]["next"].asString();
    if (!next || !isApiUrl(*next))
      return listResult::success(std::move(rows));
    request.url = *next;
  }
}

syncOutcome malListService::saveEntry(const entry& item) {
  auto id = idOf(item);
  if (!id)
    return syncOutcome::failure("'" + item.title + "' has no MyAnimeList id.");
  httpRequest request;
  request.method = "PATCH";
  request.url = std::string(kApiRoot) + kindPath(item.type) + "/" +
                std::to_string(*id) + "/my_list_status";
  request.headers["Content-Type"] = "application/x-www-form-urlencoded";
  request.body = listStatusForm(item);
  auto response = send(request, true);
  if (!response.ok())
    return syncOutcome::failure(response.errorMessage());
  if (response.value().statusCode != 200)
    return syncOutcome::failure("Saving '" + item.title + "': " +
                                apiError(response.value()));
  return syncSucceeded();
}

syncOutcome malListService::deleteEntry(const remoteListEntry& row) {
  auto id = idOf(row.item);
  if (!id)
    return syncOutcome::failure("'" + row.item.title + "' has no MyAnimeList id.");
  httpRequest request;
  request.method = "DELETE";
  request.url = std::string(kApiRoot) + kindPath(row.item.type) + "/" +
                std::to_string(*id) + "/my_list_status";
  auto response = send(request, true);
  if (!response.ok())
    return syncOutcome::failure(response.errorMessage());
  int status = response.value().statusCode;
  // 404 means it is already gone, which is what we wanted.
  if (status != 200 && status != 404)
    return syncOutcome::failure("Deleting '" + row.item.title + "': " +
                                apiError(response.value()));
  return syncSucceeded();
}

}  // namespace catalist
