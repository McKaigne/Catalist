#ifndef CATALIST_NET_PROVIDERS_I_PROVIDER_HPP_
#define CATALIST_NET_PROVIDERS_I_PROVIDER_HPP_

#include <string>
#include <vector>

#include "catalist/model/entry.hpp"

namespace catalist {

struct providerSearchResult {
  std::string providerKey;
  std::string remoteId;
  std::string title;
  mediaType type = mediaType::kAnime;
  int totalUnits = 0;
  double score = 0.0;
  std::string year;
  std::string synopsis;
};

class iProvider {
public:
  virtual ~iProvider() = default;
  virtual std::string getKey() const = 0;
  virtual std::string getDisplayName() const = 0;
  virtual std::string getShortBadge() const = 0;
  virtual std::string getWebUrl(mediaType type,
                                const std::string& remoteId) const = 0;
  virtual std::vector<providerSearchResult> search(mediaType type,
                                                   const std::string& query,
                                                   size_t limit) = 0;
};

}  // namespace catalist

#endif  // CATALIST_NET_PROVIDERS_I_PROVIDER_HPP_
