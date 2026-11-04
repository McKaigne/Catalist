#pragma once

#include <ftxui/screen/color.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalist/entry.hpp"
#include "catalist/platform/httpClient.hpp"

namespace catalist {

struct ProviderSearchResult {
  std::string providerKey;
  std::string remoteId;
  std::string title;
  MediaType type = MediaType::Anime;
  int totalUnits = 0;
  double score = 0.0;
  std::string year;
  std::string synopsis;
};

class IProvider {
public:
  virtual ~IProvider() = default;
  virtual std::string getKey() const = 0;
  virtual std::string getDisplayName() const = 0;
  virtual std::string getShortBadge() const = 0;
  virtual ftxui::Color getBrandColor() const = 0;
  virtual std::string getWebUrl(MediaType type,
                                const std::string& remoteId) const = 0;
  virtual std::vector<ProviderSearchResult> search(MediaType type,
                                                   const std::string& query,
                                                   size_t limit) = 0;
};

class ProviderRegistry {
public:
  static ProviderRegistry& get();

  ProviderRegistry();
  explicit ProviderRegistry(std::shared_ptr<IHttpClient> client);

  void registerProvider(std::shared_ptr<IProvider> provider);
  std::shared_ptr<IProvider> getProvider(const std::string& key) const;
  std::vector<std::shared_ptr<IProvider>> getAllProviders() const;

  std::string resolveUrl(const Entry& entry,
                         const std::string& preferredProvider = "") const;

private:
  std::shared_ptr<IHttpClient> httpClient_;
  std::unordered_map<std::string, std::shared_ptr<IProvider>> providers_;
  std::vector<std::string> providerOrder_;
};

}  // namespace catalist
