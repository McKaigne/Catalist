#include "catalist/provider.hpp"

#include <algorithm>
#include <cctype>

#include "catalist/remoteSearch.hpp"

namespace catalist {

namespace {

std::string extractJsonString(const std::string& block,
                              const std::string& key) {
  std::string pattern = "\"" + key + "\":\"";
  size_t pos = block.find(pattern);
  if (pos == std::string::npos) {
    pattern = "\"" + key + "\": \"";
    pos = block.find(pattern);
    if (pos == std::string::npos)
      return "";
  }
  size_t start = pos + pattern.size();
  size_t end = start;
  while (end < block.size()) {
    if (block[end] == char(34) && (end == 0 || block[end - 1] != char(92)))
      break;
    end++;
  }
  return (end > start) ? block.substr(start, end - start) : "";
}

double extractJsonNumber(const std::string& block, const std::string& key) {
  std::string pattern = "\"" + key + "\":";
  size_t pos = block.find(pattern);
  if (pos == std::string::npos) {
    pattern = "\"" + key + "\": ";
    pos = block.find(pattern);
    if (pos == std::string::npos)
      return 0.0;
  }
  size_t start = pos + pattern.size();
  while (start < block.size() &&
         (block[start] == char(32) || block[start] == char(9) ||
          block[start] == char(34))) {
    start++;
  }
  if (start < block.size() && block[start] == char(110))
    return 0.0;
  size_t end = start;
  while (end < block.size() &&
         (std::isdigit(static_cast<unsigned char>(block[end])) ||
          block[end] == char(46))) {
    end++;
  }
  if (end > start) {
    try {
      return std::stod(block.substr(start, end - start));
    } catch (...) {
    }
  }
  return 0.0;
}

}  // namespace

class MalProvider : public IProvider {
public:
  std::string getKey() const override { return "mal"; }
  std::string getDisplayName() const override { return "MyAnimeList"; }
  std::string getShortBadge() const override { return "MAL"; }
  ftxui::Color getBrandColor() const override {
    return ftxui::Color::RGB(46, 81, 162);
  }

  std::string getWebUrl(MediaType type,
                        const std::string& remoteId) const override {
    if (remoteId.empty())
      return "";
    return (type == MediaType::Anime)
               ? "https://myanimelist.net/anime/" + remoteId
               : "https://myanimelist.net/manga/" + remoteId;
  }

  std::vector<ProviderSearchResult> search(MediaType type,
                                           const std::string& query,
                                           size_t limit) override {
    std::string err;
    auto rawResults = RemoteSearchEngine::query(type, query, err, limit);
    std::vector<ProviderSearchResult> results;
    for (const auto& r : rawResults) {
      if (r.malId > 0) {
        results.push_back({getKey(), std::to_string(r.malId), r.title, r.type,
                           r.totalUnits, r.score, r.year, r.synopsis});
      }
    }
    return results;
  }
};

class AnilistProvider : public IProvider {
public:
  std::string getKey() const override { return "anilist"; }
  std::string getDisplayName() const override { return "AniList"; }
  std::string getShortBadge() const override { return "AL"; }
  ftxui::Color getBrandColor() const override {
    return ftxui::Color::RGB(2, 169, 255);
  }

  std::string getWebUrl(MediaType type,
                        const std::string& remoteId) const override {
    if (remoteId.empty())
      return "";
    return (type == MediaType::Anime) ? "https://anilist.co/anime/" + remoteId
                                      : "https://anilist.co/manga/" + remoteId;
  }

  std::vector<ProviderSearchResult> search(MediaType type,
                                           const std::string& query,
                                           size_t limit) override {
    std::string err;
    auto rawResults = RemoteSearchEngine::query(type, query, err, limit);
    std::vector<ProviderSearchResult> results;
    for (const auto& r : rawResults) {
      results.push_back({getKey(), std::to_string(r.malId), r.title, r.type,
                         r.totalUnits, r.score, r.year, r.synopsis});
    }
    return results;
  }
};

class KitsuProvider : public IProvider {
public:
  explicit KitsuProvider(
      std::shared_ptr<IHttpClient> client = std::make_shared<CurlHttpClient>())
      : client_(std::move(client)) {}

  std::string getKey() const override { return "kitsu"; }
  std::string getDisplayName() const override { return "Kitsu"; }
  std::string getShortBadge() const override { return "KT"; }
  ftxui::Color getBrandColor() const override {
    return ftxui::Color::RGB(224, 80, 43);
  }

  std::string getWebUrl(MediaType type,
                        const std::string& remoteId) const override {
    if (remoteId.empty())
      return "";
    return (type == MediaType::Anime) ? "https://kitsu.app/anime/" + remoteId
                                      : "https://kitsu.app/manga/" + remoteId;
  }

  std::vector<ProviderSearchResult> search(MediaType type,
                                           const std::string& query,
                                           size_t limit) override {
    if (query.empty() || !client_)
      return {};

    std::string path = (type == MediaType::Anime) ? "anime" : "manga";
    HttpRequest req;
    req.url = "https://kitsu.io/api/edge/" + path +
              "?filter[text]=" + RemoteSearchEngine::urlEncode(query) +
              "&page[limit]=" + std::to_string(limit);
    req.headers["Accept"] = "application/vnd.api+json";

    auto resp = client_->execute(req);
    if (!resp.ok() || resp.value().body.empty())
      return {};

    const std::string& json = resp.value().body;
    size_t dataPos = json.find("\"data\":[");
    if (dataPos == std::string::npos)
      dataPos = json.find("\"data\": [");
    if (dataPos == std::string::npos)
      return {};

    std::vector<ProviderSearchResult> results;
    size_t cur = dataPos + 7;
    while (cur < json.size() && results.size() < limit) {
      size_t objStart = json.find(char(123), cur);
      if (objStart == std::string::npos)
        break;

      int depth = 0;
      size_t objEnd = std::string::npos;
      bool inQuote = false;
      for (size_t i = objStart; i < json.size(); ++i) {
        if (json[i] == char(34) && (i == 0 || json[i - 1] != char(92))) {
          inQuote = !inQuote;
        } else if (!inQuote) {
          if (json[i] == char(123)) {
            depth++;
          } else if (json[i] == char(125)) {
            depth--;
            if (depth == 0) {
              objEnd = i;
              break;
            }
          }
        }
      }

      if (objEnd == std::string::npos)
        break;
      std::string block = json.substr(objStart, objEnd - objStart + 1);
      cur = objEnd + 1;

      std::string id = extractJsonString(block, "id");
      if (id.empty())
        continue;
      std::string title = extractJsonString(block, "canonicalTitle");
      if (title.empty())
        title = extractJsonString(block, "en_jp");
      if (title.empty())
        continue;

      ProviderSearchResult item;
      item.providerKey = getKey();
      item.remoteId = id;
      item.title = title;
      item.type = type;
      item.totalUnits = static_cast<int>(extractJsonNumber(
          block, type == MediaType::Anime ? "episodeCount" : "chapterCount"));
      double rating = extractJsonNumber(block, "averageRating");
      item.score = rating > 0.0 ? rating / 10.0 : 0.0;
      results.push_back(std::move(item));
    }
    return results;
  }

private:
  std::shared_ptr<IHttpClient> client_;
};

ProviderRegistry& ProviderRegistry::get() {
  static ProviderRegistry instance;
  return instance;
}

ProviderRegistry::ProviderRegistry()
    : httpClient_(std::make_shared<CurlHttpClient>()) {
  registerProvider(std::make_shared<MalProvider>());
  registerProvider(std::make_shared<AnilistProvider>());
  registerProvider(std::make_shared<KitsuProvider>(httpClient_));
}

ProviderRegistry::ProviderRegistry(std::shared_ptr<IHttpClient> client)
    : httpClient_(std::move(client)) {
  if (!httpClient_) {
    httpClient_ = std::make_shared<CurlHttpClient>();
  }
  registerProvider(std::make_shared<MalProvider>());
  registerProvider(std::make_shared<AnilistProvider>());
  registerProvider(std::make_shared<KitsuProvider>(httpClient_));
}

void ProviderRegistry::registerProvider(std::shared_ptr<IProvider> provider) {
  if (!provider)
    return;
  std::string key = provider->getKey();
  providers_[key] = provider;
  if (std::find(providerOrder_.begin(), providerOrder_.end(), key) ==
      providerOrder_.end()) {
    providerOrder_.push_back(key);
  }
}

std::shared_ptr<IProvider> ProviderRegistry::getProvider(
    const std::string& key) const {
  auto it = providers_.find(key);
  if (it != providers_.end())
    return it->second;
  return nullptr;
}

std::vector<std::shared_ptr<IProvider>> ProviderRegistry::getAllProviders()
    const {
  std::vector<std::shared_ptr<IProvider>> res;
  for (const auto& key : providerOrder_) {
    auto it = providers_.find(key);
    if (it != providers_.end())
      res.push_back(it->second);
  }
  return res;
}

std::string ProviderRegistry::resolveUrl(
    const Entry& entry, const std::string& preferredProvider) const {
  if (!preferredProvider.empty()) {
    std::string rid = entry.getRemoteId(preferredProvider);
    if (!rid.empty()) {
      auto p = getProvider(preferredProvider);
      if (p)
        return p->getWebUrl(entry.type, rid);
    }
  }

  for (const auto& key : providerOrder_) {
    std::string rid = entry.getRemoteId(key);
    if (!rid.empty()) {
      auto p = getProvider(key);
      if (p)
        return p->getWebUrl(entry.type, rid);
    }
  }

  return "";
}

}  // namespace catalist
