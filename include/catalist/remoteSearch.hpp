#pragma once

#include "catalist/entry.hpp"
#include <string>
#include <vector>

namespace catalist {

struct RemoteSearchResult {
  int malId = 0;
  std::string title;
  std::string titleEnglish;
  MediaType type = MediaType::Anime;
  int totalUnits = 0;
  double score = 0.0;
  std::string year;
  std::string status;
  std::string synopsis;
};

class RemoteSearchEngine {
public:
  static std::vector<RemoteSearchResult> query(MediaType type, const std::string& searchQuery, std::string& outErr, size_t limit = 10);
  static std::string urlEncode(const std::string& value);
};

} // namespace catalist
