#ifndef CATALIST_MENU_COMMON_HPP_
#define CATALIST_MENU_COMMON_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "menu/app.hpp"

namespace catalist {

inline constexpr size_t kPageSize = 20;
inline constexpr int kStatusCount = 6;
inline constexpr int kStatusesForNewTitle = 5;

void Fail(App& app, const std::string& message);
void Done(App& app, const std::string& message);
void Warn(App& app, const std::string& message);
void Note(App& app, const std::string& message);
void ClearScreen(App& app);
// The big logo, or the small one on a narrower terminal, or none.
void DrawLogo(App& app);
// Breadcrumbs joined by " / ".
void DrawHeading(App& app, const std::string& heading);
void DrawOption(App& app, int number, int digits, const std::string& label);
// Numbered from 1, then 0 for `leave`.
void DrawOptions(App& app, const std::vector<std::string>& items,
                 const std::string& leave = "Back");
void DrawMenu(App& app, const std::string& heading, const std::vector<std::string>& items);

// A label in a dim column `width` wide, then the value.
void CardRow(App& app, const std::string& label, const std::string& value, int width = 14);

[[nodiscard]] const char* StatusColor(WatchStatus status);
// [W] Watching, [C] Completed, [H] On Hold, [D] Dropped, [P] Planned and
// [R] Rewatching.
[[nodiscard]] std::string StatusMark(WatchStatus status);
[[nodiscard]] std::string StatusLabel(WatchStatus status, MediaType type);
[[nodiscard]] std::string UnitName(MediaType type, int count = 2);
[[nodiscard]] std::string OutOf(int done, int total);

[[nodiscard]] std::optional<WatchStatus> AskStatus(App& app, const std::string& heading, int count,
                                                   MediaType type);

// Applies the change, saves it and prints what it changed.
void ChangeTitle(App& app, const Entry& title, const std::function<Entry(const Entry&)>& change);

void ListScreen(App& app, MediaType type);
void TitleScreen(App& app, const Entry& title);
void AddScreen(App& app, MediaType type);
void TransferScreen(App& app);

}  // namespace catalist

#endif  // CATALIST_MENU_COMMON_HPP_
