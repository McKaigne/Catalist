#ifndef CATALIST_REFS_HPP_
#define CATALIST_REFS_HPP_

#include <optional>
#include <string>
#include <vector>

#include "catalogue.hpp"
#include "entry.hpp"

// How the command line names a title in the lists: `a:12` or `m:12` (anime
// or manga, by the number the list gave it), or any other text, which is
// looked for in the titles.
namespace catalist::refs {

struct localRef {
	mediaType type = mediaType::kAnime;
	int id = 0;
};

// `a:12` or `m:12`; nothing for any other text.
[[nodiscard]] std::optional<localRef> parse(const std::string& text);
// The ref of an entry: "a:12".
[[nodiscard]] std::string of(const entry& named);
[[nodiscard]] std::string of(mediaType type, int id);

// Whether the title holds the text, in any case, as the tracker's `/` filter
// does. Empty text is in every title.
[[nodiscard]] bool titleHolds(const entry& shown, const std::string& text);

// The entries the text names, anime first, each list in its stored order:
// the one with that ref, else every title that holds the text; a title that
// is exactly the text wins over those that only hold it. `only` keeps the
// search to one list.
[[nodiscard]] std::vector<entry> find(const catalogue& db, const std::string& text,
                                      std::optional<mediaType> only);

}  // namespace catalist::refs

#endif  // CATALIST_REFS_HPP_
