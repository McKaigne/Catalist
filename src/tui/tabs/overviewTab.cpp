#include "catalist/tui/tabs/overviewTab.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

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

}  // namespace

std::string overviewTab::id() const {
  return "overview";
}

std::string overviewTab::title() const {
  return "Overview";
}

std::string overviewTab::icon() const {
  return icon::kStats;
}

Element overviewTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  return vbox({text("Statistics") | bold | color(theme.text), text(""),
               mediaSection(ctx.db, mediaType::kAnime, "Anime Stats",
                            theme.lavender, theme),
               text(""),
               mediaSection(ctx.db, mediaType::kManga, "Manga Stats",
                            theme.peach, theme),
               filler()}) |
         size(WIDTH, EQUAL, 102) | center | flex;
}

bool overviewTab::onEvent(const Event&, appContext&) {
  return false;
}

std::vector<hint> overviewTab::hints() const {
  return {{"Tab", "tabs"}, {":", "cmd"}, {"q", "quit"}};
}

}  // namespace catalist
