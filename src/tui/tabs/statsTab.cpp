#include "catalist/tui/tabs/statsTab.hpp"

#include <map>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

using namespace ftxui;

namespace {

std::string formatCommas(long long number) {
  std::string digits = std::to_string(number);
  for (int insertAt = static_cast<int>(digits.size()) - 3; insertAt > 0;
       insertAt -= 3)
    digits.insert(static_cast<size_t>(insertAt), ",");
  return digits;
}

std::string formatDays(double days) {
  long long whole = static_cast<long long>(days);
  int tenths = static_cast<int>(std::round((days - whole) * 10.0)) % 10;
  return formatCommas(whole) + "." + std::to_string(tenths);
}

std::string formatMean(double mean) {
  std::ostringstream formatted;
  formatted << std::fixed << std::setprecision(2) << mean;
  return formatted.str();
}

struct statusCounts {
  size_t watching, completed, onHold, dropped, planned, total;
};

statusCounts countStatuses(const database& db, mediaType type) {
  return {db.countByStatus(type, watchStatus::kWatching),
          db.countByStatus(type, watchStatus::kCompleted),
          db.countByStatus(type, watchStatus::kOnHold),
          db.countByStatus(type, watchStatus::kDropped),
          db.countByStatus(type, watchStatus::kPlanToWatch),
          db.getEntries(type).size()};
}

// Every non-empty status keeps at least one cell so it stays visible.
int segmentWidth(size_t count, size_t total, int width) {
  int cells = static_cast<int>((count * width) / total);
  return count > 0 && cells == 0 ? 1 : cells;
}

Element statusBar(const statusCounts& counts, const themePalette& theme) {
  const int width = 46;
  if (counts.total == 0)
    return text(std::string(width, ' ')) | bgcolor(theme.surface0);
  const std::pair<size_t, Color> segments[] = {{counts.watching, theme.green},
                                               {counts.completed, theme.blue},
                                               {counts.onHold, theme.yellow},
                                               {counts.dropped, theme.red}};
  Elements cells;
  int used = 0;
  for (const auto& [count, tint] : segments) {
    int cellsWide = segmentWidth(count, counts.total, width);
    used += cellsWide;
    if (cellsWide > 0)
      cells.push_back(text(std::string(cellsWide, ' ')) | bgcolor(tint));
  }
  int planned = std::max(0, width - used);
  if (planned > 0)
    cells.push_back(text(std::string(planned, ' ')) | bgcolor(theme.overlay0));
  return hbox(std::move(cells));
}

Element legendRow(const std::string& label, size_t count, Color dot,
                  const themePalette& theme) {
  return hbox({text("● ") | color(dot), text(label) | color(theme.blue),
               filler(), text(formatCommas(count)) | color(theme.text)});
}

Element totalRow(const std::string& label, long long value,
                 const themePalette& theme) {
  return hbox({text(label) | color(theme.subtext), filler(),
               text(formatCommas(value)) | color(theme.text)});
}

Element recentActivity(const database& db, mediaType type,
                       const themePalette& theme) {
  bool isAnime = type == mediaType::kAnime;
  Elements rows = {
      hbox({text(isAnime ? "Last Anime Updates" : "Last Manga Updates") | bold |
                color(theme.text),
            filler(),
            text(isAnime ? "Anime History" : "Manga History") |
                color(theme.blue)}),
      text("")};
  auto recent = db.getRecentActivity(type, 3);
  if (recent.empty())
    rows.push_back(text(" No recent history logged.") | color(theme.subtext));

  for (const auto& activity : recent) {
    float ratio = activity.totalUnits > 0
                      ? std::clamp(static_cast<float>(activity.progress) /
                                       activity.totalUnits,
                                   0.0f, 1.0f)
                      : 0.0f;
    const int gaugeWidth = 14;
    int filled = static_cast<int>(std::round(ratio * gaugeWidth));
    Color fill = activity.status == watchStatus::kCompleted ? theme.blue
                                                            : theme.green;
    auto miniBar =
        hbox({text(std::string(filled, ' ')) | bgcolor(fill),
              text(std::string(std::max(0, gaugeWidth - filled), ' ')) |
                  bgcolor(theme.surface1)});
    std::string summary =
        statusToString(activity.status, activity.type) + " " +
        std::to_string(activity.progress) + "/" +
        (activity.totalUnits > 0 ? std::to_string(activity.totalUnits) : "?") +
        " · Scored " +
        (activity.score > 0 ? std::to_string(activity.score) : "-");
    rows.push_back(
        vbox({text(marquee::clip(activity.title, 42)) | bold | color(theme.blue),
              hbox({miniBar, filler(),
                    text(activity.timestamp) | color(theme.overlay0)}),
              text(summary) | color(theme.subtext), text("")}));
  }
  return vbox(std::move(rows)) | size(WIDTH, EQUAL, 46);
}

Element mediaSection(const database& db, mediaType type,
                     const std::string& heading, Color accent,
                     const themePalette& theme) {
  bool isAnime = type == mediaType::kAnime;
  auto counts = countStatuses(db, type);
  auto legend =
      vbox({legendRow(isAnime ? "Watching" : "Reading", counts.watching,
                      theme.green, theme),
            legendRow("Completed", counts.completed, theme.blue, theme),
            legendRow("On-Hold", counts.onHold, theme.yellow, theme),
            legendRow("Dropped", counts.dropped, theme.red, theme),
            legendRow(isAnime ? "Plan to Watch" : "Plan to Read",
                      counts.planned, theme.overlay0, theme)}) |
      size(WIDTH, EQUAL, 23);
  auto totals =
      vbox({totalRow("Total Entries", static_cast<long long>(counts.total),
                     theme),
            totalRow(isAnime ? "Rewatched" : "Reread", db.totalRewatched(type),
                     theme),
            totalRow(isAnime ? "Episodes" : "Chapters", db.totalProgress(type),
                     theme),
            isAnime ? text("")
                    : totalRow("Volumes", db.totalVolumes(mediaType::kManga),
                               theme)}) |
      size(WIDTH, EQUAL, 20);

  auto statsPane =
      vbox({hbox({text(heading) | bold | color(accent), filler(),
                  text("All " + heading) | color(theme.blue)}),
            text(""),
            hbox({text("Days: ") | color(theme.subtext),
                  text(formatDays(db.daysSpent(type))) | bold |
                      color(theme.text),
                  filler(), text("Mean Score: ") | color(theme.subtext),
                  text(formatMean(db.averageScore(type))) | bold |
                      color(theme.text)}),
            text(""), statusBar(counts, theme), text(""),
            hbox({legend, text("   "), totals})}) |
      size(WIDTH, EQUAL, 48);

  return hbox({statsPane, text("    "), separator() | color(theme.surface0),
               text("    "), recentActivity(db, type, theme)});
}

// One horizontal bar per label, scaled to the largest count.
Element barChart(const std::vector<std::pair<std::string, size_t>>& rows,
                 Color tint, const themePalette& theme) {
  const int barWidth = 30;
  size_t largest = 1;
  for (const auto& row : rows)
    largest = std::max(largest, row.second);
  Elements lines;
  for (const auto& [label, count] : rows) {
    int filled = static_cast<int>((count * barWidth) / largest);
    if (count > 0 && filled == 0)
      filled = 1;
    lines.push_back(hbox({text(label) | size(WIDTH, EQUAL, 7) |
                              color(theme.subtext),
                          text(std::string(filled, ' ')) | bgcolor(tint),
                          text(" " + formatCommas(static_cast<long long>(count))) |
                              color(theme.text)}));
  }
  return vbox(std::move(lines));
}

std::vector<std::pair<std::string, size_t>> scoreHistogram(const database& db,
                                                           mediaType type) {
  std::vector<std::pair<std::string, size_t>> rows;
  for (int score = 10; score >= 1; --score)
    rows.emplace_back(std::to_string(score) + " ★", db.countByScore(type, score));
  return rows;
}

std::vector<std::pair<std::string, size_t>> entriesPerYear(const database& db,
                                                           mediaType type) {
  const size_t shownYears = 10;
  std::map<int, size_t> perYear;
  size_t undated = 0;
  for (const auto& e : db.getEntries(type)) {
    if (e.startDate)
      perYear[e.startDate->year()]++;
    else
      undated++;
  }
  std::vector<std::pair<std::string, size_t>> rows;
  for (auto year = perYear.rbegin();
       year != perYear.rend() && rows.size() < shownYears; ++year)
    rows.emplace_back(std::to_string(year->first), year->second);
  if (undated > 0)
    rows.emplace_back("none", undated);
  return rows;
}

Element chartCard(const std::string& heading, Element chart,
                  const themePalette& theme) {
  return vbox({text(" " + heading) | bold | color(theme.lavender), text(""),
               chart}) |
         border | color(theme.surface0) | flex;
}

}  // namespace

std::string statsTab::id() const {
  return "stats";
}

std::string statsTab::title() const {
  return "Stats";
}

std::string statsTab::icon() const {
  return icon::kStats;
}

Element statsTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  bool isAnime = media_ == mediaType::kAnime;
  auto toggle = hbox({text(" Anime ") | (isAnime ? bold | color(theme.mauve)
                                                 : color(theme.subtext)),
                      text("│") | color(theme.surface1),
                      text(" Manga ") | (isAnime ? color(theme.subtext)
                                                 : bold | color(theme.mauve))});
  auto charts =
      hbox({chartCard("Score Histogram",
                      barChart(scoreHistogram(ctx.db, media_), theme.yellow, theme),
                      theme),
            text(" "),
            chartCard("Entries per Start Year",
                      barChart(entriesPerYear(ctx.db, media_), theme.blue, theme),
                      theme)});
  return vbox({hbox({text("Statistics") | bold | color(theme.text), filler(),
                     toggle}),
               text(""),
               mediaSection(ctx.db, media_,
                            isAnime ? "Anime Stats" : "Manga Stats",
                            isAnime ? theme.lavender : theme.peach, theme),
               text(""), charts, filler()}) |
         size(WIDTH, EQUAL, 102) | center | flex;
}

bool statsTab::onEvent(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  bool toggles = keys::isBound(event, keybinds, keyAction::kToggleMedia) ||
                 keys::isMoveLeft(event, keybinds) ||
                 keys::isMoveRight(event, keybinds);
  if (!toggles)
    return false;
  media_ = media_ == mediaType::kAnime ? mediaType::kManga : mediaType::kAnime;
  return true;
}

std::vector<hint> statsTab::hints() const {
  return {{"m/h/l", "anime/manga"}, {":", "cmd"}, {"q", "quit"}};
}

}  // namespace catalist
