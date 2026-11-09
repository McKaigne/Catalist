#pragma once

#include <string>
#include <vector>

#include "catalist/entry.hpp"

namespace catalist {

struct remoteSearchResult {
  int malId = 0;
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
  static std::vector<remoteSearchResult> query(mediaType type,
                                               const std::string& searchQuery,
                                               std::string& outErr,
                                               size_t limit = 10);
  static std::string urlEncode(const std::string& value);
};

}  // namespace catalist
