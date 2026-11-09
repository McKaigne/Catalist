#ifndef CATALIST_NET_PROVIDERS_ANILIST_PROVIDER_HPP_
#define CATALIST_NET_PROVIDERS_ANILIST_PROVIDER_HPP_

#include <memory>
#include <string>
#include <vector>

#include "catalist/net/providers/iProvider.hpp"
#include "catalist/platform/httpClient.hpp"

namespace catalist {

class anilistProvider : public iProvider {
public:
  explicit anilistProvider(std::shared_ptr<iHttpClient> client)
      : client_(std::move(client)) {}

  [[nodiscard]] std::string getKey() const override;
  [[nodiscard]] std::string getDisplayName() const override;
  [[nodiscard]] std::string getShortBadge() const override;
  [[nodiscard]] std::string getWebUrl(
      mediaType type, const std::string& remoteId) const override;
  [[nodiscard]] std::vector<providerSearchResult> search(
      mediaType type, const std::string& query, size_t limit) override;

private:
  std::shared_ptr<iHttpClient> client_;
};

}  // namespace catalist

#endif  // CATALIST_NET_PROVIDERS_ANILIST_PROVIDER_HPP_
