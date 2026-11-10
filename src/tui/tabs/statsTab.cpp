#include "catalist/tui/tabs/statsTab.hpp"

#include <algorithm>
#include <map>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/widgets/card.hpp"
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

Element statusBar(const statusCounts& counts) {
  const int width = 46;
  if (counts.total == 0)
    return text(std::string(width, ' ')) | bgcolor(theme::kSurface0);
  const std::pair<size_t, Color> segments[] = {{counts.watching, theme::kGreen},
                                               {counts.completed, theme::kBlue},
                                               {counts.onHold, theme::kYellow},
                                               {counts.dropped, theme::kRed}};
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
    cells.push_back(text(std::string(planned, ' ')) | bgcolor(theme::kOverlay0));
  return hbox(std::move(cells));
}

Element legendRow(const std::string& label, size_t count, Color dot) {
  return hbox({text("  ● ") | color(dot), text(label) | color(theme::kText),
               filler(),
               text("(" + formatCommas(static_cast<long long>(count)) + ")  ") |
                   color(theme::kSubtext)});
}

Element sidebarCard(const database& db, mediaType selected) {
  bool isAnime = selected == mediaType::kAnime;
  auto counts = countStatuses(db, selected);
  auto mediaChoice = [&](mediaType type, const std::string& label) {
    std::string count =
        " (" + std::to_string(db.getEntries(type).size()) + ")";
    return card::choice(type == selected, label + count,
                        type == selected ? theme::kMauve : theme::kText);
  };
  auto body = vbox({card::section("MEDIA"),
                    mediaChoice(mediaType::kAnime, icon::kAnime + " Anime"),
                    mediaChoice(mediaType::kManga, icon::kManga + " Manga"),
                    text(""), card::section("STATUS"),
                    legendRow(isAnime ? "Watching" : "Reading", counts.watching,
                              theme::kGreen),
                    legendRow("Completed", counts.completed, theme::kBlue),
                    legendRow("On-Hold", counts.onHold, theme::kYellow),
                    legendRow("Dropped", counts.dropped, theme::kRed),
                    legendRow(isAnime ? "Plan to Watch" : "Plan to Read",
                              counts.planned, theme::kOverlay0),
                    filler()});
  return card::frame("Statistics", body, true) | size(WIDTH, EQUAL, 28);
}

Element metric(const std::string& label, const std::string& value) {
  return card::field(false, label, text(value) | bold | color(theme::kText), 16);
}

Element overviewCard(const database& db, mediaType type) {
  bool isAnime = type == mediaType::kAnime;
  auto counts = countStatuses(db, type);
  auto left = vbox(
      {metric("Days spent:", formatDays(db.daysSpent(type))),
       metric("Mean score:", formatMean(db.averageScore(type))),
       metric("Entries:", formatCommas(static_cast<long long>(counts.total)))});
  auto right = vbox(
      {metric(isAnime ? "Episodes:" : "Chapters:",
              formatCommas(db.totalProgress(type))),
       metric(isAnime ? "Rewatched:" : "Reread:",
              formatCommas(db.totalRewatched(type))),
       isAnime ? text("")
               : metric("Volumes:", formatCommas(db.totalVolumes(type)))});
  auto body = vbox({card::section("TOTALS"), hbox({left | flex, right | flex}),
                    text(""), card::section("BY STATUS"),
                    hbox({text("  "), statusBar(counts), filler()})});
  return card::frame(isAnime ? "ANIME OVERVIEW" : "MANGA OVERVIEW", body,
                     false);
}

Element activityItem(const activityRecord& activity) {
  float ratio = activity.totalUnits > 0
                    ? std::clamp(static_cast<float>(activity.progress) /
                                     activity.totalUnits,
                                 0.0f, 1.0f)
                    : 0.0f;
  Color fill =
      activity.status == watchStatus::kCompleted ? theme::kGreen : theme::kBlue;
  std::string summary =
      statusToString(activity.status, activity.type) + " · " +
      std::to_string(activity.progress) + "/" +
      (activity.totalUnits > 0 ? std::to_string(activity.totalUnits) : "?") +
      " · " + (activity.score > 0 ? std::to_string(activity.score) + "/10" : "unscored");
  return vbox({text("  " + marquee::clip(activity.title, 36)) | bold |
                   color(theme::kMauve),
               text("  " + activity.timestamp) | color(theme::kOverlay0),
               hbox({text("  "), gauge(ratio) | color(fill) | flex, text("  ")}),
               text("  " + summary) | color(theme::kSubtext), text("")});
}

Element activityCard(const database& db, mediaType type) {
  Elements rows;
  for (const auto& activity : db.getRecentActivity(type, 6))
    rows.push_back(activityItem(activity));
  if (rows.empty())
    rows.push_back(card::note("No updates logged yet."));
  return card::frame("RECENT ACTIVITY", vbox(std::move(rows)) | frame, false) |
         size(WIDTH, EQUAL, 42);
}

Element barChart(const std::vector<std::pair<std::string, size_t>>& rows,
                 Color tint) {
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
                              color(theme::kSubtext),
                          text(std::string(filled, ' ')) | bgcolor(tint),
                          text(" " + formatCommas(static_cast<long long>(count))) |
                              color(theme::kText)}));
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

Element chartCard(const std::string& heading, Element chart) {
  return card::frame(heading, hbox({text("  "), std::move(chart)}), false) | flex;
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
  auto charts =
      hbox({chartCard("SCORES", barChart(scoreHistogram(ctx.db, media_),
                                         theme::kYellow)),
            chartCard("START YEARS", barChart(entriesPerYear(ctx.db, media_),
                                              theme::kBlue))});
  return hbox({sidebarCard(ctx.db, media_),
               vbox({overviewCard(ctx.db, media_), charts | flex}) | flex,
               activityCard(ctx.db, media_)});
}

bool statsTab::onEvent(const Event& event, appContext&) {
  bool toggles = keys::isCharacter(event, 'm') ||
                 keys::isMoveUp(event) || keys::isMoveDown(event);
  if (!toggles)
    return false;
  media_ = media_ == mediaType::kAnime ? mediaType::kManga : mediaType::kAnime;
  return true;
}

std::vector<hint> statsTab::hints() const {
  return {{"Tab", "tabs"}, {"j/k/m", "anime/manga"}, {"q", "quit"}};
}

}  // namespace catalist
