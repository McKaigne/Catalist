#ifndef CATALIST_MODEL_SYNC_BASE_HPP_
#define CATALIST_MODEL_SYNC_BASE_HPP_

#include <string>

#include "catalist/model/entry.hpp"

namespace catalist {

// The list values (status, score, progress, volumes, rewatches, dates and
// notes) one title had on one service when Catalist and the service last
// agreed. Sync compares both sides with it to tell which one changed.
struct syncBase {
  std::string service;
  int remoteId = 0;
  // Only the type and the list values are kept.
  entry values;
};

}  // namespace catalist

#endif  // CATALIST_MODEL_SYNC_BASE_HPP_
