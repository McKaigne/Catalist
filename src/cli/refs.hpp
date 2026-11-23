#ifndef CATALIST_CLI_REFS_HPP_
#define CATALIST_CLI_REFS_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "storage/catalogue.hpp"

// How the command line names a title in the lists: `a:12` or `m:12` (anime
// or manga, by the number the list gave it), or any other text, which is
// looked for in the titles.
namespace catalist::refs {

struct LocalRef {
	MediaType type = MediaType::kAnime;
	int id = 0;
};

// `a:12` or `m:12`; nothing for any other text.
[[nodiscard]] std::optional<LocalRef> Parse(const std::string& text);
// The ref of an entry: "a:12".
[[nodiscard]] std::string Of(const Entry& named);
[[nodiscard]] std::string Of(MediaType type, int id);

// Whether its English or romaji name holds every word of the text, in any
// order, ignoring case and punctuation. Empty text is in every title.
[[nodiscard]] bool TitleHolds(const Entry& shown, const std::string& text);

// The entries the text names, anime first, each list in its stored order:
// the one with that ref, else every title that holds the text; a title that
// is exactly the text wins over those that only hold it. `only` keeps the
// search to one list.
[[nodiscard]] std::vector<Entry> Find(const Catalogue& db, const std::string& text,
                                      std::optional<MediaType> only);

// The title the text was most likely a misspelling of, if any is close.
[[nodiscard]] std::optional<Entry> Closest(const Catalogue& db, const std::string& text,
                                           std::optional<MediaType> only);

}  // namespace catalist::refs

#endif  // CATALIST_CLI_REFS_HPP_
