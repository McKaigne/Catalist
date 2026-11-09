#include "catalist/net/providers/malProvider.hpp"

#include "catalist/net/providers/engineSearch.hpp"

namespace catalist {

namespace {

int malIdOf(const remoteSearchResult& raw) {
  return raw.malId;
}

}  // namespace

std::string malProvider::getKey() const {
  return "mal";
}

std::string malProvider::getDisplayName() const {
  return "MyAnimeList";
}

std::string malProvider::getShortBadge() const {
  return "MAL";
}

std::string malProvider::getWebUrl(mediaType type,
                          const std::string& remoteId) const {
  if (remoteId.empty())
    return "";
  std::string kind = type == mediaType::kAnime ? "anime" : "manga";
  return "https://myanimelist.net/" + kind + "/" + remoteId;
}

std::vector<providerSearchResult> malProvider::search(mediaType type,
                                             const std::string& query,
                                             size_t limit) {
  return searchViaEngine(*client_, getKey(), type, query, limit, malIdOf);
}

}  // namespace catalist
