#pragma once

#include <string>

#include "catalist/platform/fileSystem.hpp"
#include "catalist/storage/database.hpp"

namespace catalist {

class malParser {
public:
  static size_t importXml(const iFileSystem& fileSystem,
                          const std::string& filepath, database& db,
                          bool clean = false);
};

}  // namespace catalist
