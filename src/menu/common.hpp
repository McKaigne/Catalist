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
void ClearScreen(App& app);
void DrawHeading(App& app, const std::string& heading);
void DrawOptions(App& app, const std::vector<std::string>& items,
                 const std::string& leave = "Back");
void DrawMenu(App& app, const std::string& heading, const std::vector<std::string>& items);

[[nodiscard]] const char* StatusColor(WatchStatus status);
// With no type, the name that covers both anime and manga.
[[nodiscard]] std::string StatusLabel(WatchStatus status, std::optional<MediaType> type);
[[nodiscard]] std::string UnitName(MediaType type, int count = 2);
[[nodiscard]] std::string OutOf(int done, int total);
[[nodiscard]] std::vector<Entry> InProgress(const Catalogue& db);

[[nodiscard]] std::optional<WatchStatus> AskStatus(App& app, const std::string& heading, int count,
                                                   std::optional<MediaType> type);
[[nodiscard]] std::optional<Entry> ChooseFrom(App& app, std::vector<Entry> titles);
[[nodiscard]] std::optional<Entry> ChooseTitle(App& app);

// Applies the change, saves it and prints what it changed.
void ChangeTitle(App& app, const Entry& title, const std::function<Entry(const Entry&)>& change);

void AddScreen(App& app);
void ViewScreen(App& app);
void StatsScreen(App& app);
void PrintCard(App& app, const Entry& e);
void LogProgressScreen(App& app);
void EditScreen(App& app);
void RemoveScreen(App& app);
void TransferScreen(App& app);

}  // namespace catalist

#endif  // CATALIST_MENU_COMMON_HPP_
