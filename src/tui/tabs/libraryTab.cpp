#include "catalist/tui/tabs/libraryTab.hpp"

#include <algorithm>
#include <set>

#include "catalist/tui/actions.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/statusStyle.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

using namespace ftxui;

namespace {

const std::vector<watchStatus>& allStatuses() {
  static const std::vector<watchStatus> statuses = {
      watchStatus::kWatching, watchStatus::kCompleted, watchStatus::kOnHold,
      watchStatus::kDropped, watchStatus::kPlanToWatch};
  return statuses;
}

std::optional<int> startYear(const entry& e) {
  return e.startDate ? std::optional<int>(e.startDate->year()) : std::nullopt;
}

std::string yearLabel(const std::optional<int>& year) {
  return year ? std::to_string(*year) : "No date";
}

Element listRow(const std::string& label, const std::string& count,
                bool cursorHere, bool columnFocused, const themePalette& theme) {
  bool highlighted = cursorHere && columnFocused;
  auto row = hbox({text(cursorHere ? "▌ " : "  ") | color(theme.mauve) | bold,
                   text(label) | (cursorHere ? bold : nothing) |
                       color(highlighted ? theme.peach : theme.text) | flex,
                   text(count + " ") | color(theme.overlay0)});
  return highlighted ? row | bgcolor(theme.surface0) | focus : row;
}

Element columnCard(const std::string& heading, Elements rows, bool focused,
                   const themePalette& theme) {
  if (rows.empty())
    rows.push_back(text("  (empty)") | color(theme.overlay0));
  return vbox({text(" " + heading) | bold | color(theme.lavender), text(""),
               vbox(std::move(rows)) | vscroll_indicator | frame | flex}) |
         border | color(focused ? theme.mauve : theme.surface0);
}

}  // namespace

std::string libraryTab::id() const {
  return "library";
}

std::string libraryTab::title() const {
  return "Library";
}

std::string libraryTab::icon() const {
  return icon::kLibrary;
}

void libraryTab::onActivate(appContext& ctx) {
  refresh(ctx);
}

std::vector<entry> libraryTab::entriesFor(
    const database& db, std::optional<int> year,
    std::optional<watchStatus> status) const {
  std::vector<entry> matching;
  for (const auto& e : db.getEntries(media_)) {
    if (startYear(e) != year)
      continue;
    if (status && e.status != *status)
      continue;
    matching.push_back(e);
  }
  std::sort(matching.begin(), matching.end(),
            [](const entry& a, const entry& b) { return a.title < b.title; });
  return matching;
}

void libraryTab::refresh(appContext& ctx) {
  std::set<std::optional<int>> seen;
  for (const auto& e : ctx.db.getEntries(media_))
    seen.insert(startYear(e));
  years_.assign(seen.begin(), seen.end());
  // Newest year first; undated entries (nullopt sorts first) go last.
  std::sort(years_.begin(), years_.end(), [](const auto& a, const auto& b) {
    if (a.has_value() != b.has_value())
      return a.has_value();
    return a > b;
  });
  yearCursor_ = std::clamp(yearCursor_, 0, std::max(0, lastIndexFor(column::kYears)));

  statuses_ = allStatuses();
  statusCursor_ = std::clamp(statusCursor_, 0, lastIndexFor(column::kStatuses));

  entries_.clear();
  if (!years_.empty())
    entries_ = entriesFor(ctx.db, years_[yearCursor_], statuses_[statusCursor_]);
  entryCursor_ = std::clamp(entryCursor_, 0,
                            std::max(0, lastIndexFor(column::kEntries)));
}

int& libraryTab::cursorFor(column which) {
  if (which == column::kYears)
    return yearCursor_;
  if (which == column::kStatuses)
    return statusCursor_;
  return entryCursor_;
}

int libraryTab::lastIndexFor(column which) const {
  size_t count = which == column::kYears      ? years_.size()
                 : which == column::kStatuses ? statuses_.size()
                                              : entries_.size();
  return std::max(0, static_cast<int>(count) - 1);
}

bool libraryTab::onEvent(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  int focusIndex = static_cast<int>(focus_);
  int& cursor = cursorFor(focus_);
  int last = lastIndexFor(focus_);
  bool moved = true;
  if (keys::isMoveLeft(event, keybinds))
    focus_ = static_cast<column>(std::max(focusIndex - 1, 0));
  else if (keys::isMoveRight(event, keybinds))
    focus_ = static_cast<column>(std::min(focusIndex + 1, 2));
  else if (keys::isMoveDown(event, keybinds))
    cursor = std::min(cursor + 1, last);
  else if (keys::isMoveUp(event, keybinds))
    cursor = std::max(cursor - 1, 0);
  else if (keys::isJumpTop(event, keybinds))
    cursor = 0;
  else if (keys::isJumpBottom(event, keybinds))
    cursor = last;
  else if (keys::isBound(event, keybinds, keyAction::kToggleMedia))
    media_ = media_ == mediaType::kAnime ? mediaType::kManga : mediaType::kAnime;
  else
    moved = false;
  if (moved) {
    if (focus_ != column::kEntries)
      entryCursor_ = std::min(entryCursor_, lastIndexFor(column::kEntries));
    refresh(ctx);
    return true;
  }
  bool opens = focus_ == column::kEntries && !entries_.empty() &&
               keys::isBound(event, keybinds, keyAction::kOpenBrowser);
  if (opens)
    openInBrowser(ctx, entries_[entryCursor_]);
  return opens;
}

Element libraryTab::render(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  Elements yearRows;
  for (size_t i = 0; i < years_.size(); ++i) {
    size_t count = entriesFor(ctx.db, years_[i], std::nullopt).size();
    yearRows.push_back(listRow(yearLabel(years_[i]), std::to_string(count),
                               static_cast<int>(i) == yearCursor_,
                               focus_ == column::kYears, theme));
  }

  Elements statusRows;
  std::optional<int> year =
      years_.empty() ? std::nullopt : years_[yearCursor_];
  for (size_t i = 0; i < statuses_.size(); ++i) {
    size_t count =
        years_.empty() ? 0 : entriesFor(ctx.db, year, statuses_[i]).size();
    statusRows.push_back(
        listRow(statusToIcon(statuses_[i]) + " " +
                    statusToString(statuses_[i], media_),
                std::to_string(count), static_cast<int>(i) == statusCursor_,
                focus_ == column::kStatuses, theme));
  }

  Elements entryRows;
  for (size_t i = 0; i < entries_.size(); ++i) {
    const auto& e = entries_[i];
    std::string progress = std::to_string(e.progress) + "/" +
                           (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?");
    entryRows.push_back(listRow(marquee::clip(e.title, 60), progress,
                                static_cast<int>(i) == entryCursor_,
                                focus_ == column::kEntries, theme));
  }

  std::string media = media_ == mediaType::kAnime ? "Anime" : "Manga";
  return hbox({columnCard(media + " · Start Year", std::move(yearRows),
                          focus_ == column::kYears, theme) |
                   size(WIDTH, EQUAL, 24),
               columnCard("Status", std::move(statusRows),
                          focus_ == column::kStatuses, theme) |
                   size(WIDTH, EQUAL, 28),
               columnCard("Entries", std::move(entryRows),
                          focus_ == column::kEntries, theme) |
                   flex});
}

std::vector<hint> libraryTab::hints() const {
  return {{"h/l", "column"}, {"j/k g/G", "move"}, {"m", "anime/manga"},
          {"o", "open"},     {"q", "quit"}};
}

}  // namespace catalist
