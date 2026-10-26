#pragma once

#include "catalist/entry.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <ftxui/screen/color.hpp>

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
  virtual std::string getShortBadge() const = 0; // "MAL", "AL", "KT"
  virtual ftxui::Color getBrandColor() const = 0;
  virtual std::string getWebUrl(MediaType type, const std::string& remoteId) const = 0;
  virtual std::vector<ProviderSearchResult> search(MediaType type, const std::string& query, size_t limit) = 0;
};

class ProviderRegistry {
public:
  static ProviderRegistry& get();

  void registerProvider(std::shared_ptr<IProvider> provider);
  std::shared_ptr<IProvider> getProvider(const std::string& key) const;
  std::vector<std::shared_ptr<IProvider>> getAllProviders() const;

  std::string resolveUrl(const Entry& entry, const std::string& preferredProvider = "") const;

private:
  ProviderRegistry();
  std::unordered_map<std::string, std::shared_ptr<IProvider>> providers_;
  std::vector<std::string> providerOrder_;
};

} // namespace catalist
