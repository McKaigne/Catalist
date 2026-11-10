#include "catalist/net/remoteSearch.hpp"

#include "catalist/core/json.hpp"
#include "catalist/core/url.hpp"

namespace catalist {

namespace {

constexpr auto kSearchTimeout = std::chrono::milliseconds(8000);

std::string stringField(const json::value& item) {
  return item.asString().value_or("");
}

int intField(const json::value& item) {
  return static_cast<int>(item.asNumber().value_or(0.0));
}

std::string unitKeyFor(mediaType type) {
  return type == mediaType::kAnime ? "episodes" : "chapters";
}

std::string anilistPayload(mediaType type, const std::string& searchQuery,
                           size_t limit) {
  json::value variables;
  variables.set("s", searchQuery);
  variables.set("t", type == mediaType::kAnime ? "ANIME" : "MANGA");
  json::value payload;
  payload.set("query", "query($s:String,$t:MediaType){Page(perPage:" +
                           std::to_string(limit) +
                           "){media(search:$s,type:$t){id idMal title{romaji "
                           "english} episodes chapters status averageScore "
                           "startDate{year}}}}");
  payload.set("variables", std::move(variables));
  return json::serialize(payload);
}

std::optional<remoteSearchResult> parseAnilistMedia(const json::value& item,
                                                    mediaType type) {
  remoteSearchResult parsed;
  parsed.type = type;
  parsed.malId = intField(item["idMal"]);
  parsed.anilistId = intField(item["id"]);
  if (parsed.malId <= 0 && parsed.anilistId <= 0)
    return std::nullopt;

  parsed.title = stringField(item["title"]["romaji"]);
  if (parsed.title.empty())
    parsed.title = stringField(item["title"]["english"]);
  if (parsed.title.empty())
    return std::nullopt;

  parsed.titleEnglish = stringField(item["title"]["english"]);
  parsed.totalUnits = intField(item[unitKeyFor(type)]);
  double averageOutOf100 = item["averageScore"].asNumber().value_or(0.0);
  parsed.score = averageOutOf100 > 0.0 ? averageOutOf100 / 10.0 : 0.0;
  parsed.status = stringField(item["status"]);
  int startYear = intField(item["startDate"]["year"]);
  if (startYear > 1900)
    parsed.year = std::to_string(startYear);
  return parsed;
}

std::optional<remoteSearchResult> parseJikanItem(const json::value& item,
                                                 mediaType type) {
  remoteSearchResult parsed;
  parsed.type = type;
  parsed.malId = intField(item["mal_id"]);
  if (parsed.malId <= 0)
    return std::nullopt;

  parsed.title = stringField(item["title"]);
  if (parsed.title.empty())
    return std::nullopt;

  parsed.titleEnglish = stringField(item["title_english"]);
  parsed.totalUnits = intField(item[unitKeyFor(type)]);
  parsed.score = item["score"].asNumber().value_or(0.0);
  parsed.status = stringField(item["status"]);

  int startYear = intField(item["year"]);
  // Anime carry their first air date under "aired", manga under "published".
  std::string firstDate = stringField(item["aired"]["from"]);
  if (firstDate.empty())
    firstDate = stringField(item["published"]["from"]);
  if (startYear > 1900)
    parsed.year = std::to_string(startYear);
  else if (firstDate.size() >= 4)
    parsed.year = firstDate.substr(0, 4);
  return parsed;
}

template <typename parser>
std::vector<remoteSearchResult> parseItems(const json::value& items,
                                           mediaType type, size_t limit,
                                           parser parseItem) {
  std::vector<remoteSearchResult> results;
  for (const auto& item : items.items()) {
    if (results.size() >= limit)
      break;
    if (auto parsed = parseItem(item, type))
      results.push_back(std::move(*parsed));
  }
  return results;
}

// A failed request and a reply that is not JSON both read as null, which has
// no items.
json::value fetchDocument(iHttpClient& http, const httpRequest& request) {
  auto response = http.execute(request);
  if (!response.ok())
    return {};
  auto document = json::parse(response.value().body);
  return document.ok() ? document.value() : json::value();
}

}  // namespace

std::vector<remoteSearchResult> remoteSearchEngine::queryAniList(
    mediaType type, const std::string& searchQuery, size_t limit) const {
  httpRequest request;
  request.url = "https://graphql.anilist.co";
  request.method = "POST";
  request.headers["Content-Type"] = "application/json";
  request.headers["User-Agent"] = "Mozilla/5.0";
  request.body = anilistPayload(type, searchQuery, limit);
  request.timeout = kSearchTimeout;

  json::value document = fetchDocument(http_, request);
  return parseItems(document["data"]["Page"]["media"], type, limit,
                    parseAnilistMedia);
}

std::vector<remoteSearchResult> remoteSearchEngine::queryJikan(
    mediaType type, const std::string& searchQuery, size_t limit) const {
  std::string endpoint = type == mediaType::kAnime ? "anime" : "manga";
  httpRequest request;
  request.url = "https://api.jikan.moe/v4/" + endpoint +
                "?q=" + url::encode(searchQuery) +
                "&limit=" + std::to_string(limit);
  request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)";
  request.timeout = kSearchTimeout;

  json::value document = fetchDocument(http_, request);
  return parseItems(document["data"], type, limit, parseJikanItem);
}

std::vector<remoteSearchResult> remoteSearchEngine::query(
    mediaType type, const std::string& searchQuery, std::string& outErr,
    size_t limit) const {
  if (searchQuery.empty())
    return {};

  auto results = queryAniList(type, searchQuery, limit);
  if (!results.empty())
    return results;

  results = queryJikan(type, searchQuery, limit);
  if (!results.empty())
    return results;

  outErr =
      "Failed to contact remote providers (AniList and Jikan). Check your "
      "internet connection.";
  return {};
}

}  // namespace catalist
