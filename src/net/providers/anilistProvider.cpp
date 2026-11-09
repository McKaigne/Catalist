#include "catalist/net/providers/anilistProvider.hpp"

#include "catalist/net/providers/engineSearch.hpp"

namespace catalist {

namespace {

int anilistIdOf(const remoteSearchResult& raw) {
  return raw.anilistId;
}

}  // namespace

std::string anilistProvider::getKey() const {
  return "anilist";
}

std::string anilistProvider::getDisplayName() const {
  return "AniList";
}

std::string anilistProvider::getShortBadge() const {
  return "AL";
}

std::string anilistProvider::getWebUrl(mediaType type,
                          const std::string& remoteId) const {
  if (remoteId.empty())
    return "";
  std::string kind = type == mediaType::kAnime ? "anime" : "manga";
  return "https://anilist.co/" + kind + "/" + remoteId;
}

std::vector<providerSearchResult> anilistProvider::search(mediaType type,
                                             const std::string& query,
                                             size_t limit) {
  return searchViaEngine(*client_, getKey(), type, query, limit, anilistIdOf);
}

}  // namespace catalist
