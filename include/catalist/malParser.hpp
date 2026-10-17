#pragma once

#include "catalist/database.hpp"
#include <string>

namespace catalist {

class MalParser {
public:
  static size_t importXml(const std::string& filepath, Database& db, bool clean = false);
};

} // namespace catalist
