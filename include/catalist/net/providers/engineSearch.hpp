#ifndef CATALIST_NET_PROVIDERS_ENGINE_SEARCH_HPP_
#define CATALIST_NET_PROVIDERS_ENGINE_SEARCH_HPP_

#include <string>
#include <vector>

#include "catalist/net/providers/iProvider.hpp"
#include "catalist/net/remoteSearch.hpp"
#include "catalist/platform/httpClient.hpp"

namespace catalist {

// Each engine-backed provider reports the id from its own catalogue; results
// that lack one (e.g. Jikan fallbacks for AniList) cannot be linked and are
// dropped.
using remoteIdOf = int (*)(const remoteSearchResult&);

[[nodiscard]] std::vector<providerSearchResult> searchViaEngine(
    iHttpClient& http, const std::string& providerKey, mediaType type,
    const std::string& query, size_t limit, remoteIdOf remoteId);

}  // namespace catalist

#endif  // CATALIST_NET_PROVIDERS_ENGINE_SEARCH_HPP_
