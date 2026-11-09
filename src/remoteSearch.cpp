#include "catalist/remoteSearch.hpp"

#include <sstream>

#include "catalist/core/jsonLite.hpp"
#include "catalist/core/url.hpp"

namespace catalist {

namespace {

using jsonLite::extractJsonNumber;
using jsonLite::extractJsonString;
using jsonLite::unescapeJsonString;

constexpr auto kSearchTimeout = std::chrono::milliseconds(8000);

std::string stringField(const std::string& block, const char* key) {
  return extractJsonString(block, key).value_or("");
}

std::string decodedField(const std::string& block, const char* key) {
  return unescapeJsonString(stringField(block, key));
}

int intField(const std::string& block, const std::string& key) {
  return static_cast<int>(extractJsonNumber(block, key).value_or(0.0));
}

std::string unitKeyFor(mediaType type) {
  return type == mediaType::kAnime ? "episodes" : "chapters";
}

std::string anilistPayload(mediaType type, const std::string& searchQuery,
                           size_t limit) {
  std::string mediaKind = type == mediaType::kAnime ? "ANIME" : "MANGA";
  std::ostringstream payload;
  payload << "{\"query\":\"query($s:String,$t:MediaType){Page(perPage:"
          << limit
          << "){media(search:$s,type:$t){id idMal title{romaji english} "
             "episodes chapters status averageScore startDate{year}}}}\","
             "\"variables\":{\"s\":\""
          << jsonLite::escapeJsonString(searchQuery) << "\",\"t\":\""
          << mediaKind << "\"}}";
  return payload.str();
}

std::optional<remoteSearchResult> parseAnilistMedia(const std::string& block,
                                                    mediaType type) {
  remoteSearchResult parsed;
  parsed.type = type;
  parsed.malId = intField(block, "idMal");
  parsed.anilistId = intField(block, "id");
  if (parsed.malId <= 0 && parsed.anilistId <= 0)
    return std::nullopt;

  parsed.title = decodedField(block, "romaji");
  if (parsed.title.empty())
    parsed.title = decodedField(block, "english");
  if (parsed.title.empty())
    return std::nullopt;

  parsed.titleEnglish = decodedField(block, "english");
  parsed.totalUnits = intField(block, unitKeyFor(type));
  double averageOutOf100 = extractJsonNumber(block, "averageScore").value_or(0);
  parsed.score = averageOutOf100 > 0.0 ? averageOutOf100 / 10.0 : 0.0;
  parsed.status = stringField(block, "status");
  int startYear = intField(block, "year");
  if (startYear > 1900)
    parsed.year = std::to_string(startYear);
  return parsed;
}

std::optional<remoteSearchResult> parseJikanItem(const std::string& block,
                                                 mediaType type) {
  remoteSearchResult parsed;
  parsed.type = type;
  parsed.malId = intField(block, "mal_id");
  if (parsed.malId <= 0)
    return std::nullopt;

  parsed.title = decodedField(block, "title");
  if (parsed.title.empty())
    return std::nullopt;

  parsed.titleEnglish = decodedField(block, "title_english");
  parsed.totalUnits = intField(block, unitKeyFor(type));
  parsed.score = extractJsonNumber(block, "score").value_or(0.0);
  parsed.status = stringField(block, "status");

  int startYear = intField(block, "year");
  std::string airedFrom = stringField(block, "from");
  if (startYear > 1900)
    parsed.year = std::to_string(startYear);
  else if (airedFrom.size() >= 4)
    parsed.year = airedFrom.substr(0, 4);
  return parsed;
}

template <typename parser>
std::vector<remoteSearchResult> parseItems(const std::string& json,
                                           const char* arrayKey,
                                           mediaType type, size_t limit,
                                           parser parseItem) {
  std::vector<remoteSearchResult> results;
  for (const auto& block : jsonLite::splitArrayObjects(json, arrayKey)) {
    if (results.size() >= limit)
      break;
    if (auto parsed = parseItem(block, type))
      results.push_back(std::move(*parsed));
  }
  return results;
}

std::optional<std::string> fetchBody(iHttpClient& http,
                                     const httpRequest& request) {
  auto response = http.execute(request);
  if (!response.ok() || response.value().body.empty())
    return std::nullopt;
  return response.value().body;
}

}  // namespace

std::string remoteSearchEngine::urlEncode(const std::string& value) {
  return url::encode(value);
}

std::vector<remoteSearchResult> remoteSearchEngine::queryAniList(
    mediaType type, const std::string& searchQuery, size_t limit) const {
  httpRequest request;
  request.url = "https://graphql.anilist.co";
  request.method = "POST";
  request.headers["Content-Type"] = "application/json";
  request.headers["User-Agent"] = "Mozilla/5.0";
  request.body = anilistPayload(type, searchQuery, limit);
  request.timeout = kSearchTimeout;

  auto body = fetchBody(http_, request);
  if (!body)
    return {};
  return parseItems(*body, "media", type, limit, parseAnilistMedia);
}

std::vector<remoteSearchResult> remoteSearchEngine::queryJikan(
    mediaType type, const std::string& searchQuery, size_t limit) const {
  std::string endpoint = type == mediaType::kAnime ? "anime" : "manga";
  httpRequest request;
  request.url = "https://api.jikan.moe/v4/" + endpoint +
                "?q=" + urlEncode(searchQuery) +
                "&limit=" + std::to_string(limit);
  request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)";
  request.timeout = kSearchTimeout;

  auto body = fetchBody(http_, request);
  if (!body)
    return {};
  return parseItems(*body, "data", type, limit, parseJikanItem);
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
