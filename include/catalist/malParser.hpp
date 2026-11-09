#pragma once

#include <string>

#include "catalist/database.hpp"

namespace catalist {

class malParser {
public:
  static size_t importXml(const std::string& filepath, database& db,
                          bool clean = false);
};

}  // namespace catalist
