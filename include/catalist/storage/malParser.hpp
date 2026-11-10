#pragma once

#include <string>

#include "catalist/platform/fileSystem.hpp"
#include "catalist/storage/database.hpp"

namespace catalist {

struct malXmlExport {
  std::string xml;
  size_t written = 0;
  // Entries without a MyAnimeList id, which the import pages cannot place.
  size_t skipped = 0;
};

class malParser {
public:
  static size_t importXml(const iFileSystem& fileSystem,
                          const std::string& filepath, database& db,
                          bool clean = false);
  // The MyAnimeList export format, which both MyAnimeList's and AniList's
  // list import pages accept; one file per media type.
  [[nodiscard]] static malXmlExport exportXml(const std::vector<entry>& entries,
                                              mediaType type);
};

}  // namespace catalist
