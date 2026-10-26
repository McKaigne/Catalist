#include "catalist/provider.hpp"
#include <algorithm>

namespace catalist {

// -----------------------------------------------------------------------------
// MyAnimeList Provider
// -----------------------------------------------------------------------------
class MalProvider : public IProvider {
public:
  std::string getKey() const override { return "mal"; }
  std::string getDisplayName() const override { return "MyAnimeList"; }
  std::string getShortBadge() const override { return "MAL"; }
  ftxui::Color getBrandColor() const override { return ftxui::Color::RGB(46, 81, 162); }

  std::string getWebUrl(MediaType type, const std::string& remoteId) const override {
    if (remoteId.empty()) return "";
    return (type == MediaType::Anime)
      ? "https://myanimelist.net/anime/" + remoteId
      : "https://myanimelist.net/manga/" + remoteId;
  }

  std::vector<ProviderSearchResult> search(MediaType type, const std::string& query, size_t limit) override {
    (void)type; (void)query; (void)limit;
    return {};
  }
};

// -----------------------------------------------------------------------------
// AniList Provider
// -----------------------------------------------------------------------------
class AnilistProvider : public IProvider {
public:
  std::string getKey() const override { return "anilist"; }
  std::string getDisplayName() const override { return "AniList"; }
  std::string getShortBadge() const override { return "AL"; }
  ftxui::Color getBrandColor() const override { return ftxui::Color::RGB(2, 169, 255); }

  std::string getWebUrl(MediaType type, const std::string& remoteId) const override {
    if (remoteId.empty()) return "";
    return (type == MediaType::Anime)
      ? "https://anilist.co/anime/" + remoteId
      : "https://anilist.co/manga/" + remoteId;
  }

  std::vector<ProviderSearchResult> search(MediaType type, const std::string& query, size_t limit) override {
    (void)type; (void)query; (void)limit;
    return {};
  }
};

// -----------------------------------------------------------------------------
// Kitsu Provider
// -----------------------------------------------------------------------------
class KitsuProvider : public IProvider {
public:
  std::string getKey() const override { return "kitsu"; }
  std::string getDisplayName() const override { return "Kitsu"; }
  std::string getShortBadge() const override { return "KT"; }
  ftxui::Color getBrandColor() const override { return ftxui::Color::RGB(224, 80, 43); }

  std::string getWebUrl(MediaType type, const std::string& remoteId) const override {
    if (remoteId.empty()) return "";
    return (type == MediaType::Anime)
      ? "https://kitsu.app/anime/" + remoteId
      : "https://kitsu.app/manga/" + remoteId;
  }

  std::vector<ProviderSearchResult> search(MediaType type, const std::string& query, size_t limit) override {
    (void)type; (void)query; (void)limit;
    return {};
  }
};

// -----------------------------------------------------------------------------
// Provider Registry
// -----------------------------------------------------------------------------
ProviderRegistry& ProviderRegistry::get() {
  static ProviderRegistry instance;
  return instance;
}

ProviderRegistry::ProviderRegistry() {
  registerProvider(std::make_shared<MalProvider>());
  registerProvider(std::make_shared<AnilistProvider>());
  registerProvider(std::make_shared<KitsuProvider>());
}

void ProviderRegistry::registerProvider(std::shared_ptr<IProvider> provider) {
  if (!provider) return;
  std::string key = provider->getKey();
  providers_[key] = provider;
  if (std::find(providerOrder_.begin(), providerOrder_.end(), key) == providerOrder_.end()) {
    providerOrder_.push_back(key);
  }
}

std::shared_ptr<IProvider> ProviderRegistry::getProvider(const std::string& key) const {
  auto it = providers_.find(key);
  if (it != providers_.end()) return it->second;
  return nullptr;
}

std::vector<std::shared_ptr<IProvider>> ProviderRegistry::getAllProviders() const {
  std::vector<std::shared_ptr<IProvider>> res;
  for (const auto& key : providerOrder_) {
    auto it = providers_.find(key);
    if (it != providers_.end()) res.push_back(it->second);
  }
  return res;
}

std::string ProviderRegistry::resolveUrl(const Entry& entry, const std::string& preferredProvider) const {
  if (!preferredProvider.empty()) {
    std::string rid = entry.getRemoteId(preferredProvider);
    if (!rid.empty()) {
      auto p = getProvider(preferredProvider);
      if (p) return p->getWebUrl(entry.type, rid);
    }
  }

  for (const auto& key : providerOrder_) {
    std::string rid = entry.getRemoteId(key);
    if (!rid.empty()) {
      auto p = getProvider(key);
      if (p) return p->getWebUrl(entry.type, rid);
    }
  }

  return "";
}

} // namespace catalist
