#include "catalist/tui/tabs/historyTab.hpp"

#include <algorithm>

#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/statusStyle.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

using namespace ftxui;

namespace {

constexpr int kHeatmapWeeks = 26;

const char* weekdayName(int weekday) {
  static const char* kNames[] = {"Mon", "Tue", "Wed", "Thu",
                                 "Fri", "Sat", "Sun"};
  return kNames[weekday % 7];
}

Color heatColor(size_t count, const themePalette& theme) {
  if (count == 0)
    return theme.surface0;
  if (count <= 1)
    return theme.blue;
  if (count <= 3)
    return theme.green;
  return theme.yellow;
}

// "2026-11-04 21:57" -> "21:57"; legacy "Nov 04, 09:57 PM" -> "09:57 PM".
std::string clockTime(const std::string& timestamp) {
  size_t legacySeparator = timestamp.find(", ");
  if (legacySeparator != std::string::npos)
    return timestamp.substr(legacySeparator + 2);
  return timestamp.size() >= 16 ? timestamp.substr(11, 5) : timestamp;
}

}  // namespace

std::string historyTab::id() const {
  return "history";
}

std::string historyTab::title() const {
  return "History";
}

std::string historyTab::icon() const {
  return "󰄉";
}

void historyTab::onActivate(appContext& ctx) {
  days_ = groupHistoryByDay(ctx.db.getHistory(), localDate(ctx.clock));
  countsByDay_.clear();
  for (const auto& day : days_)
    countsByDay_[day.day.dayNumber()] = day.records.size();
  cursor_ = std::clamp(cursor_, 0, std::max(0, static_cast<int>(days_.size()) - 1));
}

bool historyTab::onEvent(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  int last = std::max(0, static_cast<int>(days_.size()) - 1);
  if (keys::isMoveDown(event, keybinds))
    cursor_ = std::min(cursor_ + 1, last);
  else if (keys::isMoveUp(event, keybinds))
    cursor_ = std::max(cursor_ - 1, 0);
  else if (keys::isJumpTop(event, keybinds))
    cursor_ = 0;
  else if (keys::isJumpBottom(event, keybinds))
    cursor_ = last;
  else
    return false;
  return true;
}

Element historyTab::renderHeatmap(const isoDate& today) const {
  const auto& theme = themeRegistry::active();
  // The grid ends with the current week; each column is Monday..Sunday.
  long lastMonday = today.dayNumber() - today.weekday();
  long firstMonday = lastMonday - 7L * (kHeatmapWeeks - 1);
  Elements weekdayRows;
  for (int weekday = 0; weekday < 7; ++weekday) {
    Elements cells = {text(std::string(weekdayName(weekday)) + " ") |
                      color(theme.overlay0)};
    for (int week = 0; week < kHeatmapWeeks; ++week) {
      long dayNumber = firstMonday + 7L * week + weekday;
      if (dayNumber > today.dayNumber()) {
        cells.push_back(text("  "));
        continue;
      }
      auto found = countsByDay_.find(dayNumber);
      size_t count = found == countsByDay_.end() ? 0 : found->second;
      cells.push_back(text("■ ") | color(heatColor(count, theme)));
    }
    weekdayRows.push_back(hbox(std::move(cells)));
  }
  auto legend = hbox({text("less ") | color(theme.overlay0),
                      text("■ ") | color(heatColor(0, theme)),
                      text("■ ") | color(heatColor(1, theme)),
                      text("■ ") | color(heatColor(2, theme)),
                      text("■ ") | color(heatColor(4, theme)),
                      text("more") | color(theme.overlay0)});
  return vbox({hbox({text(" Activity · last " + std::to_string(kHeatmapWeeks) +
                          " weeks") |
                         bold | color(theme.lavender),
                     filler(), legend, text(" ")}),
               text(""), vbox(std::move(weekdayRows)) | center}) |
         border | color(theme.surface0);
}

Element historyTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  Elements dayRows;
  for (size_t i = 0; i < days_.size(); ++i) {
    const auto& day = days_[i];
    bool active = static_cast<int>(i) == cursor_;
    auto row = hbox({text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
                     text(day.day.toString() + " " +
                          weekdayName(day.day.weekday())) |
                         (active ? bold : nothing) | color(theme.text),
                     filler(),
                     text(std::to_string(day.records.size()) + " ") |
                         color(theme.overlay0)});
    dayRows.push_back(active ? row | bgcolor(theme.surface0) | focus : row);
  }
  if (dayRows.empty())
    dayRows.push_back(text("  No history yet.") | color(theme.subtext));

  Elements detailRows;
  if (!days_.empty()) {
    for (const auto& record : days_[cursor_].records) {
      std::string progress =
          std::to_string(record.progress) + "/" +
          (record.totalUnits > 0 ? std::to_string(record.totalUnits) : "?");
      detailRows.push_back(hbox(
          {text(" " + clockTime(record.timestamp)) | size(WIDTH, EQUAL, 10) |
               color(theme.overlay0),
           text(typeToIcon(record.type) + " ") | color(typeToColor(record.type)),
           text(marquee::clip(record.title, 50)) | color(theme.text) | flex,
           text(statusToString(record.status, record.type)) |
               size(WIDTH, EQUAL, 14) | color(statusToColor(record.status)),
           text(progress) | size(WIDTH, EQUAL, 10) | color(theme.lavender),
           text(record.score > 0 ? "★ " + std::to_string(record.score) : "") |
               size(WIDTH, EQUAL, 6) | color(theme.yellow)}));
    }
  }

  auto dayList = vbox({text(" Days") | bold | color(theme.lavender), text(""),
                       vbox(std::move(dayRows)) | vscroll_indicator | frame |
                           flex}) |
                 border | color(theme.mauve) | size(WIDTH, EQUAL, 28);
  auto details = vbox({text(" Updates") | bold | color(theme.lavender),
                       text(""),
                       vbox(std::move(detailRows)) | vscroll_indicator | frame |
                           flex}) |
                 border | color(theme.surface0) | flex;
  return vbox({renderHeatmap(localDate(ctx.clock)),
               hbox({dayList, details}) | flex});
}

std::vector<hint> historyTab::hints() const {
  return {{"j/k g/G", "day"}, {":", "cmd"}, {"q", "quit"}};
}

}  // namespace catalist
