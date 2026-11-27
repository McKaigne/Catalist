#ifndef CATALIST_MENU_FIND_HPP_
#define CATALIST_MENU_FIND_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"

namespace catalist {

[[nodiscard]] bool TitleHolds(const Entry& shown, const std::string& text);
// The title nearest to the words when each is at most a few typos away.
[[nodiscard]] std::optional<Entry> ClosestTitle(const std::vector<Entry>& titles,
                                                const std::string& text);

}  // namespace catalist

#endif  // CATALIST_MENU_FIND_HPP_
