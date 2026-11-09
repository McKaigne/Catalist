#ifndef CATALIST_NET_METADATA_SYNC_HPP_
#define CATALIST_NET_METADATA_SYNC_HPP_

#include <string>

#include "catalist/platform/httpClient.hpp"
#include "catalist/storage/database.hpp"

namespace catalist {

class metadataSync {
 public:
  static size_t syncOngoing(database& db, iHttpClient& http,
                            std::string& outMsg);
};

}  // namespace catalist

#endif  // CATALIST_NET_METADATA_SYNC_HPP_
