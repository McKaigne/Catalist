#include "catalist/net/jikan.hpp"

#include "catalist/core/json.hpp"
#include "catalist/core/url.hpp"

namespace catalist::jikan {

namespace {

constexpr int kResultLimit = 10;

using titles = std::vector<title>;

int countOrZero(const json::value& field) {
  auto count = field.asInteger();
  return count && *count > 0 ? static_cast<int>(*count) : 0;
}

// Anime carry a plain year; everything else only the date the run began.
std::string yearOf(const json::value& item, mediaType type) {
  if (auto year = item["year"].asInteger())
    return std::to_string(*year);
  const char* run = type == mediaType::kAnime ? "aired" : "published";
  std::string from = item[run]["from"].asString().value_or("");
  return from.size() >= 4 ? from.substr(0, 4) : "";
}

}  // namespace

std::string searchUrl(mediaType type, const std::string& query) {
  const char* kind = type == mediaType::kAnime ? "anime" : "manga";
  return std::string("https://api.jikan.moe/v4/") + kind +
         "?q=" + url::encode(query) + "&limit=" + std::to_string(kResultLimit);
}

result<titles> parseSearch(const std::string& body, mediaType type) {
  auto reply = json::parse(body);
  if (!reply.ok() || !reply.value()["data"].isArray())
    return result<titles>::failure("unexpected reply from Jikan");

  titles found;
  for (const auto& item : reply.value()["data"].items()) {
    title match;
    match.malId = countOrZero(item["mal_id"]);
    match.name = item["title"].asString().value_or("");
    if (match.malId == 0 || match.name.empty())
      continue;
    match.totalUnits =
        countOrZero(item[type == mediaType::kAnime ? "episodes" : "chapters"]);
    match.totalVolumes = countOrZero(item["volumes"]);
    match.year = yearOf(item, type);
    match.score = item["score"].asNumber().value_or(0.0);
    found.push_back(std::move(match));
  }
  return result<titles>::success(std::move(found));
}

result<titles> search(iHttpClient& http, mediaType type,
                      const std::string& query) {
  auto body = http.get(searchUrl(type, query));
  if (!body)
    return result<titles>::failure("could not reach Jikan");
  return parseSearch(*body, type);
}

}  // namespace catalist::jikan
