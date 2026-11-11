// Drawing for catalogTab: the sidebar, the search bar, the table (or the
// title search's matches) and the details card. Keys and state are in
// catalogTab.cpp.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ftxui/screen/terminal.hpp>

#include "tui/catalogTab.hpp"

namespace catalist {

using namespace ftxui;

namespace {

// Titles get whatever the fixed columns leave; off a terminal that is 16.
size_t titleWidth() {
  return static_cast<size_t>(std::max(16, Terminal::Size().dimx - 134));
}

std::string formatListDate(const entry& e) {
  auto shown = listDate(e);
  return shown ? shown->toString() : "    -     ";
}

Element styledText(const std::string& content, Color tint, bool emphasised) {
  auto element = text(content) | color(tint);
  return emphasised ? element | bold : element;
}

// The title with every case-insensitive match of the search marked.
Element highlightMatches(const std::string& title, const std::string& query,
                         bool emphasised) {
  std::string loweredTitle = lowercase(title);
  std::string loweredQuery = lowercase(query);
  size_t match =
      query.empty() ? std::string::npos : loweredTitle.find(loweredQuery);
  if (match == std::string::npos)
    return styledText(title, theme::kText, emphasised);

  Elements parts;
  size_t consumed = 0;
  while (match != std::string::npos) {
    if (match > consumed)
      parts.push_back(styledText(title.substr(consumed, match - consumed),
                                 theme::kText, emphasised));
    parts.push_back(text(title.substr(match, loweredQuery.size())) | bold |
                    color(theme::kCrust) | bgcolor(theme::kYellow));
    consumed = match + loweredQuery.size();
    match = loweredTitle.find(loweredQuery, consumed);
  }
  if (consumed < title.size())
    parts.push_back(
        styledText(title.substr(consumed), theme::kText, emphasised));
  return hbox(std::move(parts));
}

std::string withCommas(long long number) {
  std::string digits = std::to_string(number);
  for (int at = static_cast<int>(digits.size()) - 3; at > 0; at -= 3)
    digits.insert(static_cast<size_t>(at), ",");
  return digits;
}

std::string oneDecimal(double value) {
  long long tenths = std::llround(value * 10.0);
  return withCommas(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string twoDecimals(double value) {
  char formatted[32];
  std::snprintf(formatted, sizeof(formatted), "%.2f", value);
  return formatted;
}

// One bar split by status, in the status colours; every status with titles
// keeps at least one cell so it stays visible.
Element statusBar(const libraryTotals& totals, int width) {
  if (totals.titles == 0)
    return text(std::string(width, ' ')) | bgcolor(theme::kSurface0);
  Elements cells;
  int used = 0;
  for (size_t status = 0; status < totals.byStatus.size(); ++status) {
    size_t count = totals.byStatus[status];
    bool last = status + 1 == totals.byStatus.size();
    int cellsWide =
        last ? width - used : static_cast<int>(count * width / totals.titles);
    if (count > 0 && cellsWide == 0)
      cellsWide = 1;
    cellsWide = std::min(cellsWide, width - used);
    used += cellsWide;
    if (cellsWide > 0)
      cells.push_back(text(std::string(cellsWide, ' ')) |
                      bgcolor(statusToColor(static_cast<watchStatus>(status))));
  }
  return hbox(std::move(cells));
}

Element numberBox(const textField& field) {
  return field.render("0", true) | bgcolor(theme::kSurface0) |
         size(WIDTH, EQUAL, 8);
}

}  // namespace

Element catalogTab::render(appContext&) {
  bool picking = step_ == step::kAddSearching || step_ == step::kAddPicking;
  Element middle = picking ? renderPickCard() : renderCatalogCard();
  return hbox({renderSidebarCard(),
               vbox({renderSearchCard(), middle | flex}) | flex,
               vbox({renderDetails() | flex}) | size(WIDTH, EQUAL, 42)});
}

Element catalogTab::renderSidebarRow(int row) const {
  const auto& item = sidebarRows_[row];
  if (item.isHeader)
    return text(item.label) | bold | color(theme::kOverlay0);

  bool isFilter = row == sidebarSelected_;
  bool cursorHere =
      isFilter && step_ != step::kSearch && focus_ == focusedCard::kSidebar;
  std::string prefix = isFilter ? icon::kCheckSingle + " " : "   ";
  if (cursorHere)
    return hbox({text(prefix) | color(theme::kMauve) | bold,
                 text(item.label) | bold | color(theme::kText)}) |
           bgcolor(theme::kSurface0) | focus;
  if (isFilter)
    return hbox({text(prefix) | color(theme::kGreen) | bold,
                 text(item.label) | bold | color(theme::kLavender)});
  return hbox({text("   "), text(item.label) | color(theme::kSubtext)});
}

Element catalogTab::renderSidebarCard() const {
  bool focused = step_ != step::kSearch && focus_ == focusedCard::kSidebar;
  Elements rows;
  for (size_t row = 0; row < sidebarRows_.size(); ++row)
    rows.push_back(renderSidebarRow(static_cast<int>(row)));
  // The rows scroll; the totals stay put underneath.
  return vbox({text("  Categories") | bold | color(theme::kLavender), text(""),
               vbox(std::move(rows)) | vscroll_indicator | frame | flex,
               renderTotals()}) |
         border | color(focused ? theme::kMauve : theme::kSurface0) |
         size(WIDTH, EQUAL, 28);
}

Element catalogTab::renderTotals() const {
  bool anime = media_ == mediaType::kAnime;
  auto row = [](const std::string& label, const std::string& value) {
    return hbox({text("  " + label) | color(theme::kSubtext), filler(),
                 text(value) | bold | color(theme::kText), text("  ")});
  };
  Elements lines = {
      text(""),
      text("  TOTALS") | bold | color(theme::kOverlay0),
      row("Days spent", oneDecimal(totals_.daysSpent)),
      row("Mean score", twoDecimals(totals_.meanScore)),
      row(anime ? "Episodes" : "Chapters", withCommas(totals_.units)),
      row(anime ? "Rewatched" : "Reread", withCommas(totals_.rewatched))};
  if (!anime)
    lines.push_back(row("Volumes", withCommas(totals_.volumes)));
  lines.push_back(hbox({text("  "), statusBar(totals_, 24)}));
  return vbox(std::move(lines));
}

Element catalogTab::renderSearchCard() const {
  if (isAdding())
    return hbox(
               {text(" + ") | bold | color(theme::kBlue),
                addTitle_.render("Title to add...", step_ == step::kAddTyping) |
                    flex,
                text("Add to " + lowercase(title()) + " ") |
                    color(theme::kSubtext)}) |
           border | color(theme::kBlue) | size(HEIGHT, EQUAL, 3);
  bool searching = step_ == step::kSearch;
  std::string summary = search_.value.empty()
                            ? "All entries "
                            : "Filter: '" + search_.value + "' ";
  return hbox({text(" " + icon::kSearch + " ") | color(theme::kPeach),
               search_.render("Press '/' to search...", searching) | flex,
               text(summary) | color(theme::kSubtext), text(" ")}) |
         border | color(searching ? theme::kPeach : theme::kSurface0) |
         size(HEIGHT, EQUAL, 3);
}

Element catalogTab::renderHeaderCell(const std::string& name, sortKey key,
                                     int width, bool stretches) const {
  bool active = sortBy_ == key && !isAdding();
  std::string label = " " + name;
  if (active)
    label += isSortAscending(key, reverseSort_) ? " ▲" : " ▼";
  Element cell = text(label);
  cell = stretches ? cell | flex : cell | size(WIDTH, EQUAL, width);
  return active ? cell | bold | color(theme::kMauve) | underlined
                : cell | bold | color(theme::kSubtext);
}

Element catalogTab::renderCatalogRow(int row) {
  const auto& e = displayed_[row];
  bool rowActive = step_ != step::kSearch && row == catalogSelected_ &&
                   focus_ != focusedCard::kSidebar;
  std::string shownTitle =
      rowActive ? marquee::scroll(e.title, titleWidth(),
                                  rowTimer_.elapsedFor(catalogSelected_))
                : marquee::clip(e.title, titleWidth());

  auto statusCell =
      hbox({text(statusToIcon(e.status) + " ") | color(statusToColor(e.status)),
            text(statusToString(e.status, e.type)) |
                color(statusToColor(e.status))});
  auto line =
      hbox({text(rowActive ? "▌ " : "  ") | bold | color(theme::kMauve),
            text(formatListDate(e)) | size(WIDTH, EQUAL, 10) |
                color(theme::kSubtext),
            text(" "),
            hbox({text(" " + typeToIcon(e.type) + " "),
                  highlightMatches(shownTitle, search_.value, rowActive)}) |
                flex,
            text(" "), statusCell | size(WIDTH, EQUAL, 14), text(" "),
            text(" " + renderStarMeter(e.score)) | size(WIDTH, EQUAL, 12) |
                color(theme::kYellow),
            text(" "),
            text(" " + icon::kGauge + " " + progressLabel(e)) |
                size(WIDTH, EQUAL, 12) | color(theme::kLavender),
            text(" ")});
  return rowActive ? line | bgcolor(theme::kSurface0) | focus : line;
}

Element catalogTab::renderCatalogCard() {
  auto header = hbox(
      {text("   ") | size(WIDTH, EQUAL, 3),
       renderHeaderCell("Date", sortKey::kDate, 10, false), text(" "),
       renderHeaderCell("Title", sortKey::kTitle, 28, true), text(" "),
       renderHeaderCell("Status", sortKey::kStatus, 14, false), text(" "),
       renderHeaderCell("Rating", sortKey::kScore, 12, false), text(" "),
       renderHeaderCell("Progress", sortKey::kProgress, 12, false), text(" ")});
  Element rows;
  if (displayed_.empty()) {
    rows = text(libraryEmpty_
                    ? " Nothing here yet. Press a to add a title, or run: "
                      "catalist import <file.xml>"
                    : " No matching entries found.") |
           color(theme::kSubtext) | center | flex;
  } else {
    Elements lines;
    for (size_t row = 0; row < displayed_.size(); ++row)
      lines.push_back(renderCatalogRow(static_cast<int>(row)));
    rows = vbox(std::move(lines)) | vscroll_indicator | frame | flex;
  }
  bool focused = step_ != step::kSearch && focus_ == focusedCard::kCatalog;
  return vbox({header, text(""), rows}) | border |
         color(focused ? theme::kMauve : theme::kSurface0) | flex;
}

Element catalogTab::renderPickCard() {
  auto heading = [](const std::string& name, int width) {
    Element cell = text(" " + name) | bold | color(theme::kSubtext);
    return width > 0 ? cell | size(WIDTH, EQUAL, width) : cell | flex;
  };
  auto header =
      hbox({text("   ") | size(WIDTH, EQUAL, 3), heading("Year", 10), text(" "),
            heading("Title", 0), text(" "), heading("Length", 14), text(" "),
            heading("Score", 12), text(" ")});
  if (step_ == step::kAddSearching)
    return vbox({header, text(""),
                 text(" Searching MyAnimeList...") | color(theme::kSubtext) |
                     center | flex}) |
           border | color(theme::kBlue) | flex;

  Elements lines;
  for (size_t row = 0; row <= addResults_.size(); ++row) {
    bool active = static_cast<int>(row) == addSelected_;
    Element line;
    if (row == addResults_.size()) {
      line =
          hbox({text(active ? "▌ " : "  ") | bold | color(theme::kMauve),
                text("    +     ") | color(theme::kBlue), text(" "),
                text(" Add '" + marquee::clip(addTitle_.value, titleWidth()) +
                     "' by hand") |
                    color(theme::kSubtext) | flex});
    } else {
      const auto& match = addResults_[row];
      char score[16];
      std::snprintf(score, sizeof(score), "%.2f", match.score);
      std::string length =
          (match.totalUnits > 0 ? std::to_string(match.totalUnits) : "?") +
          unitName(media_);
      std::string name =
          active ? marquee::scroll(match.name, titleWidth(),
                                   rowTimer_.elapsedFor(addSelected_))
                 : marquee::clip(match.name, titleWidth());
      line = hbox({text(active ? "▌ " : "  ") | bold | color(theme::kMauve),
                   text(match.year.empty() ? "    -" : "   " + match.year) |
                       size(WIDTH, EQUAL, 10) | color(theme::kSubtext),
                   text(" "),
                   text(" " + name) | (active ? bold : nothing) |
                       color(theme::kText) | flex,
                   text(" "),
                   text(" " + icon::kGauge + " " + length) |
                       size(WIDTH, EQUAL, 14) | color(theme::kLavender),
                   text(" "),
                   text(" " + icon::kStarFull + " " +
                        (match.score > 0 ? std::string(score) : "-")) |
                       size(WIDTH, EQUAL, 12) | color(theme::kYellow),
                   text(" ")});
    }
    lines.push_back(active ? line | bgcolor(theme::kSurface0) | focus : line);
  }
  return vbox({header, text(""),
               vbox(std::move(lines)) | vscroll_indicator | frame | flex}) |
         border | color(theme::kBlue) | flex;
}

Element catalogTab::renderDetails() {
  const entry* current = selectedEntry();
  if (!current)
    return text("No record selected.") | color(theme::kSubtext);
  const auto& e = *current;

  float ratio = e.totalUnits > 0
                    ? std::clamp(static_cast<float>(e.progress) / e.totalUnits,
                                 0.0f, 1.0f)
                    : 0.0f;
  bool finished = ratio >= 1.0f || e.status == watchStatus::kCompleted;
  bool onDetails = focus_ == focusedCard::kDetails;
  bool typing = step_ == step::kTypeNumber;
  auto fieldRow = [&](detailsField field, const std::string& label,
                      Element content) {
    bool active = onDetails && detailsCursor_ == field;
    return hbox({text(active ? "▌ " : "  ") | color(theme::kMauve) | bold,
                 text(label) | size(WIDTH, EQUAL, 12) |
                     (active ? bold : nothing) |
                     color(active ? theme::kPeach : theme::kSubtext),
                 content | flex});
  };
  auto infoRow = [&](const std::string& label, const std::string& value) {
    return hbox({text(label) | color(theme::kSubtext),
                 text(value) | color(theme::kText), filler()});
  };

  std::string shownTitle =
      marquee::scroll(e.title, 36, detailsTimer_.elapsedFor(catalogSelected_));
  std::string unfinished =
      e.status == watchStatus::kCompleted ? "Completed" : "In progress";
  auto seriesCard =
      vbox(
          {text("  DETAILS") | bold | color(theme::kLavender), text(""),
           hbox({text("  " + shownTitle) | bold | color(theme::kMauve)}),
           text(""),
           hbox({text("  Type:     ") | color(theme::kSubtext),
                 pill(typeToIcon(e.type) + " " + typeToString(e.type),
                      theme::kCrust, typeToColor(e.type)),
                 filler()}),
           infoRow("  Link:     ",
                   e.malId > 0 ? "MAL #" + std::to_string(e.malId) : "-"),
           infoRow("  Updated:  ", e.updatedAt ? e.updatedAt->toString() : "-"),
           infoRow("  Started:  ",
                   e.startDate ? e.startDate->toString() : "Not started"),
           infoRow("  Finished: ",
                   e.finishDate ? e.finishDate->toString() : unfinished)}) |
      border | color(theme::kSurface0);

  auto countRow = [&](detailsField field) {
    bool editing = typing && detailsCursor_ == field;
    Element value = editing ? numberBox(number_)
                            : text(countLabel(e, *countFieldOf(field))) |
                                  color(theme::kText);
    return fieldRow(field, fieldLabel(field), hbox({value, filler()}));
  };
  Element progressLine =
      typing && detailsCursor_ == detailsField::kProgress
          ? hbox({numberBox(number_),
                  text(" / " + countLabel(e, countField::kTotalUnits) +
                       unitName(e.type)) |
                      color(theme::kText)})
          : text(progressLabel(e) + unitName(e.type)) | color(theme::kText);
  std::string percent =
      e.totalUnits > 0 ? std::to_string(static_cast<int>(ratio * 100)) + "% "
                       : "Ongoing ";

  bool trackingActive = step_ != step::kSearch && onDetails;
  Elements tracking = {
      hbox({text("  TRACKING") | bold | color(theme::kLavender), filler(),
            text(trackingActive ? (typing ? "[Editing]" : "[Active]") : "") |
                color(theme::kPeach)}),
      text(""),
      text("  STATUS") | bold | color(theme::kOverlay0),
      fieldRow(
          detailsField::kStatus, "Status:",
          pill(statusToIcon(e.status) + " " + statusToString(e.status, e.type),
               theme::kCrust, statusToColor(e.status))),
      text(""),
      text("  METRICS") | bold | color(theme::kOverlay0),
      fieldRow(detailsField::kRating, "Rating:",
               hbox({text(renderStarMeter(e.score)) | color(theme::kYellow),
                     text(" (" + std::to_string(e.score) + "/10)") |
                         color(theme::kText)})),
      text(""),
      fieldRow(detailsField::kProgress, "Progress:",
               vbox({hbox({progressLine, filler(),
                           text(percent) | color(theme::kSubtext)}),
                     gauge(ratio) |
                         color(finished ? theme::kGreen : theme::kBlue)})),
      text(""),
      text("  COUNTS") | bold | color(theme::kOverlay0),
      countRow(detailsField::kTotal)};
  if (media_ == mediaType::kManga)
    tracking.push_back(countRow(detailsField::kVolumes));
  tracking.push_back(countRow(detailsField::kRewatched));
  tracking.push_back(filler());

  Color trackingBorder = typing           ? theme::kPeach
                         : trackingActive ? theme::kMauve
                                          : theme::kSurface0;
  return vbox({seriesCard, vbox(std::move(tracking)) | border |
                               color(trackingBorder) | flex});
}

}  // namespace catalist
