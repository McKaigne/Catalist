#ifndef CATALIST_MENU_FIND_HPP_
#define CATALIST_MENU_FIND_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "storage/catalogue.hpp"

namespace catalist {

[[nodiscard]] bool TitleHolds(const Entry& shown, const std::string& text);
// Every title holding the words; a title that is exactly the text wins.
[[nodiscard]] std::vector<Entry> FindTitles(const Catalogue& db, const std::string& text);
[[nodiscard]] std::optional<Entry> ClosestTitle(const Catalogue& db, const std::string& text);

}  // namespace catalist

#endif  // CATALIST_MENU_FIND_HPP_
