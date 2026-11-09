#pragma once

#include <string>
#include <vector>

#include "catalist/entry.hpp"
#include "catalist/platform/httpClient.hpp"

namespace catalist {

struct remoteSearchResult {
  int malId = 0;
  int anilistId = 0;
  std::string title;
  std::string titleEnglish;
  mediaType type = mediaType::kAnime;
  int totalUnits = 0;
  double score = 0.0;
  std::string year;
  std::string status;
  std::string synopsis;
};

class remoteSearchEngine {
public:
  explicit remoteSearchEngine(iHttpClient& http) : http_(http) {}

  [[nodiscard]] std::vector<remoteSearchResult> query(
      mediaType type, const std::string& searchQuery, std::string& outErr,
      size_t limit = 10) const;
  [[nodiscard]] static std::string urlEncode(const std::string& value);

private:
  [[nodiscard]] std::vector<remoteSearchResult> queryAniList(
      mediaType type, const std::string& searchQuery, size_t limit) const;
  [[nodiscard]] std::vector<remoteSearchResult> queryJikan(
      mediaType type, const std::string& searchQuery, size_t limit) const;

  iHttpClient& http_;
};

}  // namespace catalist
