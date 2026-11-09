#include "catalist/net/providers/kitsuProvider.hpp"

#include <optional>

#include "catalist/core/jsonLite.hpp"
#include "catalist/core/url.hpp"

namespace catalist {

namespace {

using jsonLite::extractJsonNumber;
using jsonLite::extractJsonString;

std::optional<providerSearchResult> parseKitsuItem(const std::string& block,
                                                   mediaType type) {
  std::string id = extractJsonString(block, "id").value_or("");
  if (id.empty())
    return std::nullopt;
  std::string title = extractJsonString(block, "canonicalTitle").value_or("");
  if (title.empty())
    title = extractJsonString(block, "en_jp").value_or("");
  if (title.empty())
    return std::nullopt;

  providerSearchResult item;
  item.providerKey = "kitsu";
  item.remoteId = id;
  item.title = title;
  item.type = type;
  const char* unitKey =
      type == mediaType::kAnime ? "episodeCount" : "chapterCount";
  item.totalUnits =
      static_cast<int>(extractJsonNumber(block, unitKey).value_or(0.0));
  double rating = extractJsonNumber(block, "averageRating").value_or(0.0);
  item.score = rating > 0.0 ? rating / 10.0 : 0.0;
  return item;
}

httpRequest searchRequest(mediaType type, const std::string& query,
                          size_t limit) {
  std::string path = type == mediaType::kAnime ? "anime" : "manga";
  httpRequest request;
  request.url = "https://kitsu.io/api/edge/" + path +
                "?filter[text]=" + url::encode(query) +
                "&page[limit]=" + std::to_string(limit);
  request.headers["Accept"] = "application/vnd.api+json";
  return request;
}

}  // namespace

std::string kitsuProvider::getKey() const {
  return "kitsu";
}

std::string kitsuProvider::getDisplayName() const {
  return "Kitsu";
}

std::string kitsuProvider::getShortBadge() const {
  return "KT";
}

std::string kitsuProvider::getWebUrl(mediaType type,
                          const std::string& remoteId) const {
  if (remoteId.empty())
    return "";
  std::string kind = type == mediaType::kAnime ? "anime" : "manga";
  return "https://kitsu.app/" + kind + "/" + remoteId;
}

std::vector<providerSearchResult> kitsuProvider::search(
    mediaType type, const std::string& query, size_t limit) {
  if (query.empty() || !client_)
    return {};
  auto response = client_->execute(searchRequest(type, query, limit));
  if (!response.ok() || response.value().body.empty())
    return {};

  std::vector<providerSearchResult> results;
  for (const auto& block :
       jsonLite::splitArrayObjects(response.value().body, "data")) {
    if (results.size() >= limit)
      break;
    if (auto item = parseKitsuItem(block, type))
      results.push_back(std::move(*item));
  }
  return results;
}

}  // namespace catalist
