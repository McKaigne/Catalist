#ifndef CATALIST_MENU_COMMON_HPP_
#define CATALIST_MENU_COMMON_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "menu/app.hpp"

namespace catalist {

inline constexpr int kStatusCount = 6;
inline constexpr int kStatusesForNewTitle = 5;

void Fail(App& app, const std::string& message);
void ClearScreen(App& app);
void DrawHeading(App& app, const std::string& heading);
void DrawMenu(App& app, const std::string& title, const std::vector<std::string>& items,
              const std::string& leave = "Back");

[[nodiscard]] const char* StatusColor(WatchStatus status);
[[nodiscard]] std::string UnitName(MediaType type, int count = 2);
[[nodiscard]] std::string OutOf(int done, int total);
[[nodiscard]] std::vector<Entry> InProgress(const Catalogue& db);

[[nodiscard]] std::optional<WatchStatus> AskStatus(App& app, const std::string& title, int count);
[[nodiscard]] std::optional<Entry> ChooseFrom(App& app, std::vector<Entry> titles);
[[nodiscard]] std::optional<Entry> ChooseTitle(App& app);

// Applies the change, saves it as one undo step and prints what it changed.
void ChangeTitle(App& app, const Entry& title, const std::string& action,
                 const std::function<Entry(const Entry&)>& change);

void AddScreen(App& app);
void ViewScreen(App& app);
void StatsScreen(App& app);
void PrintCard(App& app, const Entry& e);
void CountNextScreen(App& app);
void EditScreen(App& app);
void RemoveScreen(App& app);
void HistoryScreen(App& app);
void TransferScreen(App& app);

}  // namespace catalist

#endif  // CATALIST_MENU_COMMON_HPP_
