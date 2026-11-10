#include "catalist/tui/tabs/catalogTab.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/terminal.hpp>

#include "catalist/model/entryOps.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/statusStyle.hpp"
#include "catalist/tui/widgets/pill.hpp"

namespace catalist {

using namespace ftxui;

namespace {

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

std::string trimmed(const std::string& text) {
  auto first = text.find_first_not_of(" \t");
  if (first == std::string::npos)
    return "";
  return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

// Score, progress and date start high-to-low (see sortEntries), so for them
// the reversed order is the ascending one.
bool isSortAscending(sortKey key, bool reverse) {
  switch (key) {
    case sortKey::kScore:
    case sortKey::kProgress:
    case sortKey::kDate:
      return reverse;
    case sortKey::kTitle:
    case sortKey::kStatus:
      return !reverse;
  }
  return false;
}

std::string progressLabel(const entry& e) {
  return std::to_string(e.progress) + "/" +
         (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?");
}

std::string formatListDate(const entry& e) {
  auto shown = listDate(e);
  return shown ? shown->toString() : "    -     ";
}

std::string unitName(mediaType type) {
  return type == mediaType::kAnime ? " eps" : " chs";
}

Element styledText(const std::string& content, Color tint, bool emphasised) {
  auto element = text(content) | color(tint);
  return emphasised ? element | bold : element;
}

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

std::vector<sidebarRow> buildSidebarRows(const database& db, mediaType media) {
  auto count = [&](watchStatus status) {
    return " (" + std::to_string(db.countByStatus(media, status)) + ")";
  };
  bool isManga = media == mediaType::kManga;
  std::vector<sidebarRow> rows = {
      {"  LIBRARY", true, std::nullopt, std::nullopt},
      {icon::kLibrary + " All Media    (" +
           std::to_string(db.getEntries(media).size()) + ")",
       false, std::nullopt, std::nullopt},
      {"  STATUS", true, std::nullopt, std::nullopt},
      {icon::kWatching + (isManga ? " Reading     " : " Watching    ") +
           count(watchStatus::kWatching),
       false, watchStatus::kWatching, std::nullopt},
      {icon::kCompleted + " Completed   " + count(watchStatus::kCompleted),
       false, watchStatus::kCompleted, std::nullopt},
      {icon::kOnHold + " On-Hold     " + count(watchStatus::kOnHold), false,
       watchStatus::kOnHold, std::nullopt},
      {icon::kDropped + " Dropped     " + count(watchStatus::kDropped), false,
       watchStatus::kDropped, std::nullopt},
      {icon::kPlan + " Planned     " + count(watchStatus::kPlanToWatch), false,
       watchStatus::kPlanToWatch, std::nullopt},
      {"  RATINGS", true, std::nullopt, std::nullopt},
  };
  // Every score has its row, 10 down to 0 (unrated), so each can be filtered.
  for (int score = 10; score >= 0; --score) {
    std::string number = (score < 10 ? " " : "") + std::to_string(score);
    rows.push_back({number + " " + renderStarMeter(score) + " (" +
                        std::to_string(db.countByScore(media, score)) + ")",
                    false, std::nullopt, score});
  }
  return rows;
}

InputOption fieldStyle() {
  InputOption option;
  option.transform = [](InputState state) {
    Element field =
        state.element |
        color(state.is_placeholder ? theme::kOverlay0 : theme::kText);
    return state.focused ? field | color(theme::kPeach) : field;
  };
  return option;
}

}  // namespace

catalogTab::catalogTab(mediaType media) : media_(media) {
  idle_ = Renderer([](bool) { return text(""); });
  searchInput_ = Input(&searchQuery_, "Press '/' to search...", fieldStyle());
  addInput_ = Input(&addQuery_, "Title to add...", fieldStyle());
  InputOption notesOption;
  notesOption.cursor_position = &notesCursor_;
  notesInput_ = Input(&notesBuffer_, "Write notes here...", notesOption);
  root_ = Container::Vertical({idle_, searchInput_, addInput_, notesInput_});
}

std::string catalogTab::title() const {
  return media_ == mediaType::kAnime ? "Anime" : "Manga";
}

std::string catalogTab::icon() const {
  return media_ == mediaType::kAnime ? icon::kAnime : icon::kManga;
}

void catalogTab::onActivate(appContext& ctx) {
  refresh(ctx);
}

bool catalogTab::passesFilters(const entry& candidate) const {
  if (sidebarSelected_ >= 0 &&
      sidebarSelected_ < static_cast<int>(sidebarRows_.size())) {
    const auto& row = sidebarRows_[sidebarSelected_];
    if (row.status && candidate.status != *row.status)
      return false;
    if (row.score && candidate.score != *row.score)
      return false;
  }
  if (searchQuery_.empty())
    return true;
  return lowercase(candidate.title).find(lowercase(searchQuery_)) !=
         std::string::npos;
}

void catalogTab::refresh(appContext& ctx) {
  sidebarRows_ = buildSidebarRows(ctx.db, media_);
  libraryEmpty_ = ctx.db.getEntries(media_).empty();
  displayed_.clear();
  for (const auto& candidate : ctx.db.getEntries(media_)) {
    if (passesFilters(candidate))
      displayed_.push_back(candidate);
  }
  database::sortEntries(displayed_, sortBy_, reverseSort_);
  int lastRow = static_cast<int>(displayed_.size()) - 1;
  catalogSelected_ = std::clamp(catalogSelected_, 0, std::max(0, lastRow));
}

const entry* catalogTab::selectedEntry() const {
  bool inRange = catalogSelected_ >= 0 &&
                 catalogSelected_ < static_cast<int>(displayed_.size());
  return inRange ? &displayed_[catalogSelected_] : nullptr;
}

void catalogTab::selectId(int id) {
  for (size_t row = 0; row < displayed_.size(); ++row) {
    if (displayed_[row].id == id)
      catalogSelected_ = static_cast<int>(row);
  }
}

void catalogTab::returnToCatalog() {
  focus_ = focusedCard::kCatalog;
  mode_ = appMode::kNor;
  idle_->TakeFocus();
}

bool catalogTab::capturesInput() const {
  return pendingDeleteId_.has_value() || mode_ == appMode::kSch ||
         editingNotes_ || adding_ != addStage::kOff;
}

appMode catalogTab::mode() const {
  return pendingDeleteId_ ? appMode::kDel : mode_;
}

std::optional<std::string> catalogTab::countBadge(appContext& ctx) const {
  return std::to_string(ctx.db.getEntries(media_).size());
}

std::vector<hint> catalogTab::hints() const {
  if (pendingDeleteId_)
    return {{"y", "delete"}, {"n/Esc", "keep"}};
  if (editingNotes_)
    return {{"ret", "save"}, {"Esc", "cancel"}};
  if (mode_ == appMode::kSch)
    return {{"ret", "apply"}, {"Esc", "clear"}};
  if (adding_ == addStage::kTyping)
    return {{"ret", "search"}, {"Esc", "cancel"}};
  if (adding_ == addStage::kSearching)
    return {{"Esc", "cancel"}};
  if (adding_ == addStage::kPicking)
    return {{"j/k", "move"}, {"ret", "add"}, {"Esc", "cancel"}};
  if (focus_ == focusedCard::kDetails)
    return {{"j/k", "field"},
            {"h/l", "adjust"},
            {"e/ret", "notes"},
            {"Esc", "catalog"}};
  return {{"Tab", "tabs"},       {"h/l", "panes"},   {"e/ret", "edit"},
          {"c/t/s/r/p", "sort"}, {"S/R/P", "cycle"}, {"+/-", "ep"},
          {"a", "add"},          {"d", "del"},       {"/", "find"},
          {"q", "quit"}};
}

bool catalogTab::onEvent(const Event& event, appContext& ctx) {
  if (pendingDeleteId_)
    return handlePendingDelete(event, ctx);
  if (mode_ == appMode::kSch)
    return handleSearchTyping(event, ctx);
  if (editingNotes_)
    return handleNotesEditing(event, ctx);
  if (adding_ != addStage::kOff)
    return handleAdding(event, ctx);
  if (handleEntryKeys(event, ctx))
    return true;
  if (focus_ != focusedCard::kDetails && handleSortKeys(event, ctx))
    return true;

  bool handledByFocus = false;
  if (focus_ == focusedCard::kCatalog)
    handledByFocus = handleCatalogFocus(event, ctx);
  else if (focus_ == focusedCard::kSidebar)
    handledByFocus = handleSidebarFocus(event, ctx);
  else
    handledByFocus = handleDetailsFocus(event, ctx);
  return handledByFocus || handleCommands(event, ctx);
}

void catalogTab::onTick(appContext& ctx) {
  if (adding_ != addStage::kSearching)
    return;
  auto found = jikan::search(ctx.http, media_, addQuery_);
  addResults_ = found.ok() ? found.value() : std::vector<jikan::title>{};
  addSelected_ = 0;
  adding_ = addStage::kPicking;
  std::string quoted = "'" + marquee::clip(addQuery_, 24) + "'";
  if (!found.ok())
    ctx.status = "Could not reach MyAnimeList; the last row adds " + quoted +
                 " by hand.";
  else if (addResults_.empty())
    ctx.status = "No matches for " + quoted + "; the last row adds it by hand.";
  else
    ctx.status =
        std::to_string(addResults_.size()) + " matches for " + quoted + ".";
}

bool catalogTab::handlePendingDelete(const Event& event, appContext& ctx) {
  if (keys::isCharacter(event, 'y') || keys::isCharacter(event, 'Y')) {
    confirmDelete(ctx);
    return true;
  }
  bool cancels = keys::isCharacter(event, 'n') ||
                 keys::isCharacter(event, 'N') || event == Event::Escape;
  if (cancels) {
    pendingDeleteId_.reset();
    ctx.status = "Deletion canceled.";
  }
  return true;
}

bool catalogTab::handleSearchTyping(const Event& event, appContext& ctx) {
  if (event == Event::Escape) {
    searchQuery_.clear();
    refresh(ctx);
    returnToCatalog();
    ctx.status = "Search cleared.";
    return true;
  }
  if (keys::isConfirm(event) || event == Event::ArrowDown) {
    refresh(ctx);
    returnToCatalog();
    ctx.status = searchQuery_.empty()
                     ? "Filter cleared."
                     : "Filtered: '" + searchQuery_ + "' (" +
                           std::to_string(displayed_.size()) + " matches)";
    return true;
  }
  searchInput_->OnEvent(event);
  refresh(ctx);
  return true;
}

bool catalogTab::handleNotesEditing(const Event& event, appContext& ctx) {
  if (keys::isConfirm(event)) {
    commitNotes(ctx);
    return true;
  }
  if (event == Event::Escape) {
    editingNotes_ = false;
    mode_ = appMode::kNor;
    idle_->TakeFocus();
    ctx.status = "Notes left unchanged.";
    return true;
  }
  notesInput_->OnEvent(event);
  return true;
}

bool catalogTab::handleAdding(const Event& event, appContext& ctx) {
  if (event == Event::Escape) {
    cancelAdding(ctx);
    return true;
  }
  if (adding_ == addStage::kTyping) {
    if (!keys::isConfirm(event)) {
      addInput_->OnEvent(event);
      return true;
    }
    addQuery_ = trimmed(addQuery_);
    if (addQuery_.empty()) {
      ctx.status = "Type a title first, or [Esc] to cancel.";
      return true;
    }
    adding_ = addStage::kSearching;
    idle_->TakeFocus();
    ctx.status =
        "Searching MyAnimeList for '" + marquee::clip(addQuery_, 24) + "'...";
    return true;
  }
  if (adding_ != addStage::kPicking)
    return true;
  // The last row adds the typed title by hand.
  int lastRow = static_cast<int>(addResults_.size());
  if (keys::isMoveDown(event))
    addSelected_ = std::min(addSelected_ + 1, lastRow);
  else if (keys::isMoveUp(event))
    addSelected_ = std::max(addSelected_ - 1, 0);
  else if (keys::isJumpTop(event))
    addSelected_ = 0;
  else if (keys::isJumpBottom(event))
    addSelected_ = lastRow;
  else if (keys::isConfirm(event))
    addPicked(ctx);
  return true;
}

bool catalogTab::handleEntryKeys(const Event& event, appContext& ctx) {
  const entry* current = selectedEntry();
  bool increments = keys::isCharacter(event, 'P') || keys::isIncrement(event);
  bool acts = keys::isCharacter(event, 'S') || keys::isCharacter(event, 'R') ||
              increments || keys::isCharacter(event, '-');
  if (!acts)
    return false;
  if (!current)
    return true;

  isoDate today = localDate(ctx.clock);
  if (keys::isCharacter(event, 'S')) {
    entry changed = cycleStatus(*current, 1, today);
    commit(ctx, changed, "Status: " + statusToString(changed.status, media_));
  } else if (keys::isCharacter(event, 'R')) {
    entry changed = adjustRating(*current, 1, today);
    commit(ctx, changed, "Rating: " + renderStarMeter(changed.score));
  } else {
    entry changed = stepProgress(*current, increments ? 1 : -1, today);
    commit(ctx, changed, "Progress: " + progressLabel(changed));
  }
  return true;
}

bool catalogTab::handleSortKeys(const Event& event, appContext& ctx) {
  const std::pair<char, std::pair<sortKey, const char*>> sortKeys[] = {
      {'c', {sortKey::kDate, "Date"}},
      {'t', {sortKey::kTitle, "Title"}},
      {'s', {sortKey::kStatus, "Status"}},
      {'r', {sortKey::kScore, "Rating"}},
      {'p', {sortKey::kProgress, "Progress"}}};
  for (const auto& [key, column] : sortKeys) {
    if (!keys::isCharacter(event, key))
      continue;
    toggleSort(column.first, column.second, ctx);
    return true;
  }
  return false;
}

bool catalogTab::handleCatalogFocus(const Event& event, appContext& ctx) {
  int lastRow = std::max(0, static_cast<int>(displayed_.size()) - 1);
  if (keys::isMoveLeft(event)) {
    focus_ = focusedCard::kSidebar;
    return true;
  }
  if (keys::isMoveRight(event)) {
    focus_ = focusedCard::kDetails;
    detailsCursor_ = detailsField::kStatus;
    return true;
  }
  if (keys::isConfirm(event) || keys::isCharacter(event, 'e')) {
    focus_ = focusedCard::kDetails;
    detailsCursor_ = detailsField::kStatus;
    ctx.status =
        "Details: [j/k] Navigate properties, [h/l] Adjust, [e/ret] Notes";
    return true;
  }
  if (keys::isMoveDown(event))
    catalogSelected_ = std::min(catalogSelected_ + 1, lastRow);
  else if (keys::isMoveUp(event))
    catalogSelected_ = std::max(catalogSelected_ - 1, 0);
  else if (keys::isJumpTop(event))
    catalogSelected_ = 0;
  else if (keys::isJumpBottom(event))
    catalogSelected_ = lastRow;
  else
    return false;
  return true;
}

bool catalogTab::handleSidebarFocus(const Event& event, appContext& ctx) {
  if (keys::isMoveRight(event) || keys::isConfirm(event)) {
    focus_ = focusedCard::kCatalog;
    return true;
  }
  if (keys::isMoveDown(event)) {
    moveSidebarCursor(1, ctx);
    return true;
  }
  if (keys::isMoveUp(event)) {
    moveSidebarCursor(-1, ctx);
    return true;
  }
  return false;
}

bool catalogTab::handleDetailsFocus(const Event& event, appContext& ctx) {
  int cursor = static_cast<int>(detailsCursor_);
  if (event == Event::Escape) {
    returnToCatalog();
    return true;
  }
  if (keys::isMoveDown(event)) {
    detailsCursor_ = static_cast<detailsField>(std::min(cursor + 1, 3));
    return true;
  }
  if (keys::isMoveUp(event)) {
    detailsCursor_ = static_cast<detailsField>(std::max(cursor - 1, 0));
    return true;
  }
  if (!selectedEntry())
    return false;

  bool onNotes = detailsCursor_ == detailsField::kNotes;
  if (keys::isMoveLeft(event)) {
    if (onNotes)
      focus_ = focusedCard::kCatalog;
    else
      adjustDetailsField(ctx, -1);
    return true;
  }
  if (keys::isMoveRight(event)) {
    if (!onNotes)
      adjustDetailsField(ctx, 1);
    return true;
  }
  bool activates =
      keys::isConfirmOrSpace(event) || keys::isCharacter(event, 'e');
  if (!activates)
    return false;
  if (!onNotes) {
    adjustDetailsField(ctx, 1);
    return true;
  }
  notesBuffer_ = selectedEntry()->notes;
  // FTXUI counts the cursor in bytes; start at the end to add to the notes.
  notesCursor_ = static_cast<int>(notesBuffer_.size());
  editingNotes_ = true;
  mode_ = appMode::kEdt;
  notesInput_->TakeFocus();
  ctx.status = "Editing notes. [ret] Save  [Esc] Cancel";
  return true;
}

bool catalogTab::handleCommands(const Event& event, appContext& ctx) {
  if (keys::isCharacter(event, '/')) {
    startSearch(ctx);
    return true;
  }
  if (keys::isCharacter(event, 'a')) {
    startAdding(ctx);
    return true;
  }
  if (keys::isCharacter(event, 'd')) {
    requestDelete(ctx);
    return true;
  }
  return false;
}

void catalogTab::commit(appContext& ctx, const entry& changed,
                        const std::string& done) {
  ctx.db.updateEntry(changed);
  refresh(ctx);
  selectId(changed.id);
  ctx.save(done);
}

void catalogTab::adjustDetailsField(appContext& ctx, int direction) {
  const entry* current = selectedEntry();
  if (!current)
    return;
  isoDate today = localDate(ctx.clock);
  if (detailsCursor_ == detailsField::kStatus) {
    entry changed = cycleStatus(*current, direction, today);
    commit(ctx, changed, "Status: " + statusToString(changed.status, media_));
  } else if (detailsCursor_ == detailsField::kRating) {
    entry changed = adjustRating(*current, direction, today);
    commit(ctx, changed, "Rating: " + renderStarMeter(changed.score));
  } else {
    entry changed = stepProgress(*current, direction, today);
    commit(ctx, changed, "Progress: " + progressLabel(changed));
  }
}

// A new column starts in its natural order; choosing it again flips it.
void catalogTab::toggleSort(sortKey key, const std::string& name,
                            appContext& ctx) {
  reverseSort_ = sortBy_ == key && !reverseSort_;
  sortBy_ = key;
  refresh(ctx);
  ctx.status = "Sort: " + name +
               (isSortAscending(sortBy_, reverseSort_) ? " (Increasing ▲)"
                                                       : " (Decreasing ▼)");
}

void catalogTab::moveSidebarCursor(int direction, appContext& ctx) {
  int candidate = sidebarSelected_ + direction;
  int lastRow = static_cast<int>(sidebarRows_.size()) - 1;
  while (candidate >= 0 && candidate <= lastRow &&
         sidebarRows_[candidate].isHeader)
    candidate += direction;
  if (candidate < 1 || candidate > lastRow)
    return;
  sidebarSelected_ = candidate;
  refresh(ctx);
}

void catalogTab::startSearch(appContext& ctx) {
  mode_ = appMode::kSch;
  searchInput_->TakeFocus();
  ctx.status = "Search title... [ret] Apply  [Esc] Clear";
}

void catalogTab::commitNotes(appContext& ctx) {
  editingNotes_ = false;
  mode_ = appMode::kNor;
  idle_->TakeFocus();
  const entry* current = selectedEntry();
  if (!current)
    return;
  entry changed = *current;
  changed.notes = notesBuffer_;
  changed.updatedAt = localDate(ctx.clock);
  commit(ctx, changed, "Saved notes.");
}

void catalogTab::requestDelete(appContext& ctx) {
  const entry* current = selectedEntry();
  if (!current)
    return;
  pendingDeleteId_ = current->id;
  ctx.status =
      "Delete '" + marquee::clip(current->title, 24) + "'? Confirm [y/N]";
}

void catalogTab::confirmDelete(appContext& ctx) {
  auto doomed = ctx.db.getById(media_, *pendingDeleteId_);
  pendingDeleteId_.reset();
  if (!doomed)
    return;
  ctx.db.removeEntry(media_, doomed->id);
  refresh(ctx);
  ctx.save("Deleted '" + marquee::clip(doomed->title, 36) + "'.");
}

void catalogTab::startAdding(appContext& ctx) {
  adding_ = addStage::kTyping;
  mode_ = appMode::kAdd;
  addQuery_.clear();
  addResults_.clear();
  addInput_->TakeFocus();
  ctx.status = "Add: type a title to look up on MyAnimeList.";
}

void catalogTab::cancelAdding(appContext& ctx) {
  adding_ = addStage::kOff;
  addResults_.clear();
  addQuery_.clear();
  returnToCatalog();
  ctx.status = "Add canceled.";
}

void catalogTab::addPicked(appContext& ctx) {
  entry added;
  added.type = media_;
  added.status = watchStatus::kPlanToWatch;
  added.updatedAt = localDate(ctx.clock);
  added.title = addQuery_;
  std::optional<entry> existing;
  if (addSelected_ < static_cast<int>(addResults_.size())) {
    const auto& pick = addResults_[addSelected_];
    added.title = pick.name;
    added.malId = pick.malId;
    added.totalUnits = pick.totalUnits;
    added.totalVolumes = pick.totalVolumes;
    existing = ctx.db.findByMalId(media_, pick.malId);
  }

  adding_ = addStage::kOff;
  addResults_.clear();
  addQuery_.clear();
  // Show the whole list so the title is there to be seen.
  searchQuery_.clear();
  sidebarSelected_ = 1;
  returnToCatalog();
  std::string name = "'" + marquee::clip(added.title, 36) + "'";
  if (existing) {
    refresh(ctx);
    selectId(existing->id);
    ctx.status = name + " is already in the catalogue.";
    return;
  }
  int id = ctx.db.addEntry(added);
  refresh(ctx);
  selectId(id);
  ctx.save("Added " + name + ".");
}

Element catalogTab::renderSidebarRow(int row) const {
  const auto& item = sidebarRows_[row];
  if (item.isHeader)
    return text(item.label) | bold | color(theme::kOverlay0);

  bool isFilter = row == sidebarSelected_;
  bool cursorHere =
      isFilter && mode_ != appMode::kSch && focus_ == focusedCard::kSidebar;
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

Element catalogTab::renderCatalogRow(int row) {
  const auto& e = displayed_[row];
  bool rowActive = mode_ != appMode::kSch && row == catalogSelected_ &&
                   focus_ != focusedCard::kSidebar;
  size_t maxGlyphs =
      static_cast<size_t>(std::max(16, Terminal::Size().dimx - 134));
  std::string shownTitle =
      rowActive ? marquee::scroll(e.title, maxGlyphs,
                                  rowTimer_.elapsedFor(catalogSelected_))
                : marquee::clip(e.title, maxGlyphs);

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
                  highlightMatches(shownTitle, searchQuery_, rowActive)}) |
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

Element catalogTab::renderHeaderCell(const std::string& name, sortKey key,
                                     int width, bool stretches) const {
  bool active = sortBy_ == key && adding_ == addStage::kOff;
  std::string label = " " + name;
  if (active)
    label += isSortAscending(key, reverseSort_) ? " ▲" : " ▼";
  Element cell = text(label);
  cell = stretches ? cell | flex : cell | size(WIDTH, EQUAL, width);
  return active ? cell | bold | color(theme::kMauve) | underlined
                : cell | bold | color(theme::kSubtext);
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
  auto fieldRow = [&](detailsField field, const std::string& label,
                      Element content) {
    bool active = onDetails && detailsCursor_ == field;
    return hbox({text(active ? "▌ " : "  ") | color(theme::kMauve) | bold,
                 text(label) | size(WIDTH, EQUAL, 12) |
                     (active ? bold : nothing) |
                     color(active ? theme::kPeach : theme::kSubtext),
                 content | flex});
  };

  std::string links;
  if (e.malId > 0)
    links = "MAL #" + std::to_string(e.malId);
  if (e.anilistId > 0)
    links += (links.empty() ? "" : "  ") + std::string("AL #") +
             std::to_string(e.anilistId);

  std::string shownTitle =
      marquee::scroll(e.title, 36, detailsTimer_.elapsedFor(catalogSelected_));
  std::string unfinishedLabel =
      e.status == watchStatus::kCompleted ? "Completed" : "In progress";
  auto infoRow = [&](const std::string& label, const std::string& value) {
    return hbox({text(label) | color(theme::kSubtext),
                 text(value) | color(theme::kText), filler()});
  };

  auto seriesCard =
      vbox(
          {hbox({text("  DETAILS") | bold | color(theme::kLavender), filler(),
                 text("ID #" + std::to_string(e.id) + " ") |
                     color(theme::kOverlay0)}),
           text(""),
           hbox({text("  " + shownTitle) | bold | color(theme::kMauve)}),
           text(""),
           hbox({text("  Type:     ") | color(theme::kSubtext),
                 pill(typeToIcon(e.type) + " " + typeToString(e.type),
                      theme::kCrust, typeToColor(e.type)),
                 filler()}),
           hbox({text("  Links:    ") | color(theme::kSubtext),
                 text(links.empty() ? "-" : links) |
                     color(links.empty() ? theme::kSubtext : theme::kText),
                 filler()}),
           infoRow("  Updated:  ", e.updatedAt ? e.updatedAt->toString() : "-"),
           infoRow("  Started:  ",
                   e.startDate ? e.startDate->toString() : "Not started"),
           infoRow("  Finished: ", e.finishDate ? e.finishDate->toString()
                                                : unfinishedLabel)}) |
      border | color(theme::kSurface0);

  bool trackingActive = mode_ != appMode::kSch && onDetails &&
                        detailsCursor_ != detailsField::kNotes;
  std::string percent =
      e.totalUnits > 0 ? std::to_string(static_cast<int>(ratio * 100)) + "% "
                       : "Ongoing ";
  auto trackingCard =
      vbox(
          {hbox(
               {text("  TRACKING") | bold | color(theme::kLavender), filler(),
                text(trackingActive ? "[Active]" : "") | color(theme::kPeach)}),
           text(""), text("  STATUS") | bold | color(theme::kOverlay0),
           fieldRow(detailsField::kStatus, "Status:",
                    pill(statusToIcon(e.status) + " " +
                             statusToString(e.status, e.type),
                         theme::kCrust, statusToColor(e.status))),
           text(""), text("  METRICS") | bold | color(theme::kOverlay0),
           fieldRow(
               detailsField::kRating, "Rating:",
               hbox({text(renderStarMeter(e.score)) | color(theme::kYellow),
                     text(" (" + std::to_string(e.score) + "/10)") |
                         color(theme::kText)})),
           text(""),
           fieldRow(
               detailsField::kProgress, "Progress:",
               vbox({hbox({text(std::to_string(e.progress) + " / " +
                                (e.totalUnits > 0 ? std::to_string(e.totalUnits)
                                                  : "?") +
                                unitName(e.type)) |
                               color(theme::kText),
                           filler(), text(percent) | color(theme::kSubtext)}),
                     gauge(ratio) |
                         color(finished ? theme::kGreen : theme::kBlue)}))}) |
      border | color(trackingActive ? theme::kMauve : theme::kSurface0);

  bool notesActive = mode_ != appMode::kSch && onDetails &&
                     detailsCursor_ == detailsField::kNotes;
  Element notesBody =
      editingNotes_
          ? notesInput_->Render() | border | color(theme::kPeach)
          : text(e.notes.empty() ? "(empty - press e or ret to edit)"
                                 : e.notes) |
                color(e.notes.empty() ? theme::kOverlay0 : theme::kSubtext);
  std::string notesBadge =
      notesActive ? (editingNotes_ ? "[Editing]" : "[Active]") : "";
  Color notesBorder = notesActive
                          ? (editingNotes_ ? theme::kPeach : theme::kMauve)
                          : theme::kSurface0;
  auto notesCard =
      vbox({hbox({text("  NOTES") | bold | color(theme::kLavender), filler(),
                  text(notesBadge) | color(theme::kPeach)}),
            text(""), fieldRow(detailsField::kNotes, "Notes:", notesBody),
            filler()}) |
      border | color(notesBorder) | flex;

  return vbox({seriesCard, trackingCard, notesCard});
}

Element catalogTab::renderSidebarCard() const {
  bool focused = mode_ != appMode::kSch && focus_ == focusedCard::kSidebar;
  Elements rows;
  for (size_t row = 0; row < sidebarRows_.size(); ++row)
    rows.push_back(renderSidebarRow(static_cast<int>(row)));
  return vbox({text("  Categories") | bold | color(theme::kLavender), text(""),
               vbox(std::move(rows)) | vscroll_indicator | frame | flex}) |
         border | color(focused ? theme::kMauve : theme::kSurface0) |
         size(WIDTH, EQUAL, 28);
}

Element catalogTab::renderSearchCard() {
  if (adding_ != addStage::kOff)
    return hbox({text(" + ") | bold | color(theme::kBlue),
                 adding_ == addStage::kTyping
                     ? addInput_->Render() | flex
                     : text(addQuery_) | color(theme::kText) | flex,
                 text("Add to " + lowercase(title()) + " ") |
                     color(theme::kSubtext)}) |
           border | color(theme::kBlue) | size(HEIGHT, EQUAL, 3);
  std::string summary =
      searchQuery_.empty() ? "All entries " : "Filter: '" + searchQuery_ + "' ";
  return hbox({text(" " + icon::kSearch + " ") | color(theme::kPeach),
               searchInput_->Render() | flex,
               text(summary) | color(theme::kSubtext), text(" ")}) |
         border |
         color(mode_ == appMode::kSch ? theme::kPeach : theme::kSurface0) |
         size(HEIGHT, EQUAL, 3);
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
  bool focused = mode_ != appMode::kSch && focus_ == focusedCard::kCatalog;
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
  if (adding_ == addStage::kSearching)
    return vbox({header, text(""),
                 text(" Searching MyAnimeList...") | color(theme::kSubtext) |
                     center | flex}) |
           border | color(theme::kBlue) | flex;

  size_t maxGlyphs =
      static_cast<size_t>(std::max(16, Terminal::Size().dimx - 134));
  Elements lines;
  for (size_t row = 0; row <= addResults_.size(); ++row) {
    bool active = static_cast<int>(row) == addSelected_;
    Element line;
    if (row == addResults_.size()) {
      line = hbox(
          {text(active ? "▌ " : "  ") | bold | color(theme::kMauve),
           text("    +     ") | color(theme::kBlue), text(" "),
           text(" Add '" + marquee::clip(addQuery_, maxGlyphs) + "' by hand") |
               color(theme::kSubtext) | flex});
    } else {
      const auto& match = addResults_[row];
      char score[16];
      std::snprintf(score, sizeof(score), "%.2f", match.score);
      std::string length =
          (match.totalUnits > 0 ? std::to_string(match.totalUnits) : "?") +
          unitName(media_);
      std::string name =
          active ? marquee::scroll(match.name, maxGlyphs,
                                   rowTimer_.elapsedFor(addSelected_))
                 : marquee::clip(match.name, maxGlyphs);
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

Element catalogTab::render(appContext&) {
  Element middle =
      adding_ == addStage::kSearching || adding_ == addStage::kPicking
          ? renderPickCard()
          : renderCatalogCard();
  return hbox({renderSidebarCard(),
               vbox({renderSearchCard(), middle | flex}) | flex,
               vbox({renderDetails() | flex}) | size(WIDTH, EQUAL, 42)});
}

}  // namespace catalist
