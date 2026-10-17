#pragma once

#include "catalist/database.hpp"
#include <string>

namespace catalist {

class MetadataSync {
public:
  static size_t syncOngoing(Database& db, std::string& out_msg);
};

} // namespace catalist
