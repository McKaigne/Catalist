#pragma once

#include <string>

#include "catalist/database.hpp"

namespace catalist {

class MalParser {
public:
  static size_t importXml(const std::string& filepath, Database& db,
                          bool clean = false);
};

}  // namespace catalist
