#ifndef CATALIST_CLI_REFS_HPP_
#define CATALIST_CLI_REFS_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "storage/catalogue.hpp"

namespace catalist::refs {

struct LocalRef {
	MediaType type = MediaType::kAnime;
	int id = 0;
};

[[nodiscard]] std::optional<LocalRef> Parse(const std::string& text);
[[nodiscard]] std::string Of(const Entry& named);
[[nodiscard]] std::string Of(MediaType type, int id);

[[nodiscard]] bool TitleHolds(const Entry& shown, const std::string& text);

[[nodiscard]] std::vector<Entry> Find(const Catalogue& db, const std::string& text,
                                      std::optional<MediaType> only);

[[nodiscard]] std::optional<Entry> Closest(const Catalogue& db, const std::string& text,
                                           std::optional<MediaType> only);

}  // namespace catalist::refs

#endif  // CATALIST_CLI_REFS_HPP_
