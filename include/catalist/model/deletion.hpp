#ifndef CATALIST_MODEL_DELETION_HPP_
#define CATALIST_MODEL_DELETION_HPP_

#include <set>
#include <string>

#include "catalist/model/entry.hpp"

namespace catalist {

// An entry deleted locally after it had been synced; the next sync removes it
// from each service still listed, then forgets it.
struct deletedEntry {
  mediaType type = mediaType::kAnime;
  int malId = 0;
  int anilistId = 0;
  std::string title;
  std::set<std::string> pendingServices;
};

}  // namespace catalist

#endif  // CATALIST_MODEL_DELETION_HPP_
