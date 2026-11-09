#include "catalist/net/providers/providerRegistry.hpp"

#include <algorithm>

#include "catalist/net/providers/anilistProvider.hpp"
#include "catalist/net/providers/kitsuProvider.hpp"
#include "catalist/net/providers/malProvider.hpp"

namespace catalist {

providerRegistry::providerRegistry(std::shared_ptr<iHttpClient> client)
    : httpClient_(std::move(client)) {
  registerProvider(std::make_shared<malProvider>(httpClient_));
  registerProvider(std::make_shared<anilistProvider>(httpClient_));
  registerProvider(std::make_shared<kitsuProvider>(httpClient_));
}

void providerRegistry::registerProvider(std::shared_ptr<iProvider> provider) {
  if (!provider)
    return;
  std::string key = provider->getKey();
  providers_[key] = provider;
  if (std::find(providerOrder_.begin(), providerOrder_.end(), key) ==
      providerOrder_.end()) {
    providerOrder_.push_back(key);
  }
}

std::shared_ptr<iProvider> providerRegistry::getProvider(
    const std::string& key) const {
  auto it = providers_.find(key);
  if (it != providers_.end())
    return it->second;
  return nullptr;
}

std::vector<std::shared_ptr<iProvider>> providerRegistry::getAllProviders()
    const {
  std::vector<std::shared_ptr<iProvider>> res;
  for (const auto& key : providerOrder_) {
    auto it = providers_.find(key);
    if (it != providers_.end())
      res.push_back(it->second);
  }
  return res;
}

std::string providerRegistry::resolveUrl(
    const entry& mediaEntry, const std::string& preferredProvider) const {
  if (!preferredProvider.empty()) {
    std::string rid = mediaEntry.getRemoteId(preferredProvider);
    if (!rid.empty()) {
      auto p = getProvider(preferredProvider);
      if (p)
        return p->getWebUrl(mediaEntry.type, rid);
    }
  }

  for (const auto& key : providerOrder_) {
    std::string rid = mediaEntry.getRemoteId(key);
    if (!rid.empty()) {
      auto p = getProvider(key);
      if (p)
        return p->getWebUrl(mediaEntry.type, rid);
    }
  }

  return "";
}

}  // namespace catalist
