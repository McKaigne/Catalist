#ifndef CATALIST_STORAGE_MAL_XML_HPP_
#define CATALIST_STORAGE_MAL_XML_HPP_

#include <cstddef>
#include <string>
#include <vector>

#include "catalist/model/entry.hpp"

// The MyAnimeList list export format. MyAnimeList writes it, AniList's import
// page reads it and the usual AniList export tools write it too, so it is the
// one file format lists travel in. Pure text in and out.
namespace catalist::malXml {

// Every <anime> and <manga> block that has a title, in file order. Entries
// come back without a local id. Unknown statuses become Plan to Watch,
// scores are kept within 0..10 and unreadable numbers and dates are dropped.
[[nodiscard]] std::vector<entry> parse(const std::string& xml);

struct exported {
  std::string xml;
  size_t written = 0;
  // Entries without a MyAnimeList id, which the import pages cannot place.
  size_t skipped = 0;
};

// One file per media type, as the import pages expect.
[[nodiscard]] exported write(const std::vector<entry>& entries, mediaType type);

}  // namespace catalist::malXml

#endif  // CATALIST_STORAGE_MAL_XML_HPP_
