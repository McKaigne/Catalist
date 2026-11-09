#include "catalist/provider.hpp"
#include "catalist/core/logger.hpp"

#include <algorithm>
#include <cctype>

#include "catalist/core/jsonLite.hpp"
#include "catalist/remoteSearch.hpp"

namespace catalist {

namespace {

using jsonLite::extractJsonNumber;
using jsonLite::extractJsonString;

// Each engine-backed provider reports the id from its own catalogue; results
// that lack one (e.g. Jikan fallbacks for AniList) cannot be linked and are
// dropped.
using remoteIdOf = int (*)(const remoteSearchResult&);

std::vector<providerSearchResult> searchViaEngine(
    iHttpClient& http, const std::string& providerKey, mediaType type,
    const std::string& query, size_t limit, remoteIdOf remoteId) {
  std::string err;
  auto rawResults = remoteSearchEngine(http).query(type, query, err, limit);
  std::vector<providerSearchResult> results;
  for (const auto& raw : rawResults) {
    int id = remoteId(raw);
    if (id <= 0)
      continue;
    results.push_back({providerKey, std::to_string(id), raw.title, raw.type,
                       raw.totalUnits, raw.score, raw.year, raw.synopsis});
  }
  return results;
}

int malIdOf(const remoteSearchResult& raw) {
  return raw.malId;
}

int anilistIdOf(const remoteSearchResult& raw) {
  return raw.anilistId;
}

std::optional<providerSearchResult> parseKitsuItem(const std::string& block,
                                                   mediaType type) {
  std::string id = extractJsonString(block, "id").value_or("");
  if (id.empty())
    return std::nullopt;
  std::string title = extractJsonString(block, "canonicalTitle").value_or("");
  if (title.empty())
    title = extractJsonString(block, "en_jp").value_or("");
  if (title.empty())
    return std::nullopt;

  providerSearchResult item;
  item.providerKey = "kitsu";
  item.remoteId = id;
  item.title = title;
  item.type = type;
  const char* unitKey =
      type == mediaType::kAnime ? "episodeCount" : "chapterCount";
  item.totalUnits =
      static_cast<int>(extractJsonNumber(block, unitKey).value_or(0.0));
  double rating = extractJsonNumber(block, "averageRating").value_or(0.0);
  item.score = rating > 0.0 ? rating / 10.0 : 0.0;
  return item;
}

}  // namespace

class malProvider : public iProvider {
public:
  explicit malProvider(std::shared_ptr<iHttpClient> client)
      : client_(std::move(client)) {}

  std::string getKey() const override { return "mal"; }
  std::string getDisplayName() const override { return "MyAnimeList"; }
  std::string getShortBadge() const override { return "MAL"; }
  ftxui::Color getBrandColor() const override {
    return ftxui::Color::RGB(46, 81, 162);
  }

  std::string getWebUrl(mediaType type,
                        const std::string& remoteId) const override {
    if (remoteId.empty())
      return "";
    return (type == mediaType::kAnime)
               ? "https://myanimelist.net/anime/" + remoteId
               : "https://myanimelist.net/manga/" + remoteId;
  }

  std::vector<providerSearchResult> search(mediaType type,
                                           const std::string& query,
                                           size_t limit) override {
    return searchViaEngine(*client_, getKey(), type, query, limit, malIdOf);
  }

private:
  std::shared_ptr<iHttpClient> client_;
};

class anilistProvider : public iProvider {
public:
  explicit anilistProvider(std::shared_ptr<iHttpClient> client)
      : client_(std::move(client)) {}

  std::string getKey() const override { return "anilist"; }
  std::string getDisplayName() const override { return "AniList"; }
  std::string getShortBadge() const override { return "AL"; }
  ftxui::Color getBrandColor() const override {
    return ftxui::Color::RGB(2, 169, 255);
  }

  std::string getWebUrl(mediaType type,
                        const std::string& remoteId) const override {
    if (remoteId.empty())
      return "";
    return (type == mediaType::kAnime) ? "https://anilist.co/anime/" + remoteId
                                      : "https://anilist.co/manga/" + remoteId;
  }

  std::vector<providerSearchResult> search(mediaType type,
                                           const std::string& query,
                                           size_t limit) override {
    return searchViaEngine(*client_, getKey(), type, query, limit,
                           anilistIdOf);
  }

private:
  std::shared_ptr<iHttpClient> client_;
};

class kitsuProvider : public iProvider {
public:
  explicit kitsuProvider(std::shared_ptr<iHttpClient> client)
      : client_(std::move(client)) {}

  std::string getKey() const override { return "kitsu"; }
  std::string getDisplayName() const override { return "Kitsu"; }
  std::string getShortBadge() const override { return "KT"; }
  ftxui::Color getBrandColor() const override {
    return ftxui::Color::RGB(224, 80, 43);
  }

  std::string getWebUrl(mediaType type,
                        const std::string& remoteId) const override {
    if (remoteId.empty())
      return "";
    return (type == mediaType::kAnime) ? "https://kitsu.app/anime/" + remoteId
                                      : "https://kitsu.app/manga/" + remoteId;
  }

  std::vector<providerSearchResult> search(mediaType type,
                                           const std::string& query,
                                           size_t limit) override {
    if (query.empty() || !client_)
      return {};

    std::string path = (type == mediaType::kAnime) ? "anime" : "manga";
    httpRequest req;
    req.url = "https://kitsu.io/api/edge/" + path +
              "?filter[text]=" + remoteSearchEngine::urlEncode(query) +
              "&page[limit]=" + std::to_string(limit);
    req.headers["Accept"] = "application/vnd.api+json";

    auto resp = client_->execute(req);
    if (!resp.ok() || resp.value().body.empty())
      return {};

    std::vector<providerSearchResult> results;
    for (const auto& block :
         jsonLite::splitArrayObjects(resp.value().body, "data")) {
      if (results.size() >= limit)
        break;
      if (auto item = parseKitsuItem(block, type))
        results.push_back(std::move(*item));
    }
    return results;
  }

private:
  std::shared_ptr<iHttpClient> client_;
};

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
