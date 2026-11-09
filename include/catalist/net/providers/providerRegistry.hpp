#ifndef CATALIST_NET_PROVIDERS_PROVIDER_REGISTRY_HPP_
#define CATALIST_NET_PROVIDERS_PROVIDER_REGISTRY_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalist/model/entry.hpp"
#include "catalist/net/providers/iProvider.hpp"
#include "catalist/platform/httpClient.hpp"

namespace catalist {

class providerRegistry {
public:
  explicit providerRegistry(std::shared_ptr<iHttpClient> client);

  void registerProvider(std::shared_ptr<iProvider> provider);
  std::shared_ptr<iProvider> getProvider(const std::string& key) const;
  std::vector<std::shared_ptr<iProvider>> getAllProviders() const;

  std::string resolveUrl(const entry& mediaEntry,
                         const std::string& preferredProvider = "") const;

private:
  std::shared_ptr<iHttpClient> httpClient_;
  std::unordered_map<std::string, std::shared_ptr<iProvider>> providers_;
  std::vector<std::string> providerOrder_;
};

}  // namespace catalist

#endif  // CATALIST_NET_PROVIDERS_PROVIDER_REGISTRY_HPP_
