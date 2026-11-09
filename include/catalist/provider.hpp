#pragma once

#include <ftxui/screen/color.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalist/entry.hpp"
#include "catalist/platform/httpClient.hpp"

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
  virtual ftxui::Color getBrandColor() const = 0;
  virtual std::string getWebUrl(mediaType type,
                                const std::string& remoteId) const = 0;
  virtual std::vector<providerSearchResult> search(mediaType type,
                                                   const std::string& query,
                                                   size_t limit) = 0;
};

class providerRegistry {
public:
  static providerRegistry& get();

  providerRegistry();
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
