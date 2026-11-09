#include "catalist/net/providers/engineSearch.hpp"

namespace catalist {

std::vector<providerSearchResult> searchViaEngine(
    iHttpClient& http, const std::string& providerKey, mediaType type,
    const std::string& query, size_t limit, remoteIdOf remoteId) {
  std::string err;
  auto rawResults = remoteSearchEngine(http).query(type, query, err, limit);
  std::vector<providerSearchResult> results;
  for (const auto& raw : rawResults) {
    int id = remoteId(raw);
    if (id <= 0)
      continue;
    results.push_back({providerKey, std::to_string(id), raw.title, raw.type,
                       raw.totalUnits, raw.score, raw.year, raw.synopsis});
  }
  return results;
}

}  // namespace catalist
