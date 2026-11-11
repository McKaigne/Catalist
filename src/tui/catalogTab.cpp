#include "tui/catalogTab.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/terminal.hpp>

#include "entry.hpp"
#include "tui/style.hpp"

namespace catalist {

using namespace ftxui;

namespace {

constexpr size_t kMaxCountDigits = 6;

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

std::optional<countField> countFieldOf(detailsField field) {
  switch (field) {
    case detailsField::kProgress:
      return countField::kProgress;
    case detailsField::kTotal:
      return countField::kTotalUnits;
    case detailsField::kVolumes:
      return countField::kVolumes;
    case detailsField::kRewatched:
      return countField::kRewatched;
    case detailsField::kStatus:
    case detailsField::kRating:
      return std::nullopt;
  }
  return std::nullopt;
}

std::string knownOrUnknown(int count) {
  return count > 0 ? std::to_string(count) : "?";
}

// How a count reads in the details card and the status line.
std::string countLabel(const entry& e, countField field) {
  switch (field) {
    case countField::kProgress:
      return progressLabel(e);
    case countField::kTotalUnits:
      return knownOrUnknown(e.totalUnits);
    case countField::kVolumes:
      return std::to_string(e.volumes) + "/" + knownOrUnknown(e.totalVolumes);
    case countField::kRewatched:
      return std::to_string(e.rewatched);
  }
  return "";
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

std::vector<sidebarRow> buildSidebarRows(const catalogue& db, mediaType media) {
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
  InputOption countOption;
  countOption.cursor_position = &countCursor_;
  countInput_ = Input(&countBuffer_, "0", countOption);
  root_ = Container::Vertical({idle_, searchInput_, addInput_, countInput_});
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
  catalogue::sortEntries(displayed_, sortBy_, reverseSort_);
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
         editingCount_ || adding_ != addStage::kOff;
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
  if (editingCount_)
    return {{"ret", "save"}, {"Esc", "cancel"}};
  if (mode_ == appMode::kSch)
    return {{"ret", "apply"}, {"Esc", "clear"}};
  if (adding_ == addStage::kTyping)
    return {{"ret", "search"}, {"Esc", "cancel"}};
  // The search holds the keyboard until it answers.
  if (adding_ == addStage::kSearching)
    return {};
  if (adding_ == addStage::kPicking)
    return {{"j/k", "move"}, {"ret", "add"}, {"Esc", "cancel"}};
  if (focus_ == focusedCard::kDetails)
    return {{"j/k", "field"},
            {"h/l", "adjust"},
            {"e/ret", "type"},
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
  if (editingCount_)
    return handleCountEditing(event, ctx);
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
  auto found = jikan::search(ctx.fetch, media_, addQuery_);
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

bool catalogTab::handleCountEditing(const Event& event, appContext& ctx) {
  if (keys::isConfirm(event)) {
    commitCount(ctx);
    return true;
  }
  if (event == Event::Escape) {
    stopEditingCount();
    ctx.status = fieldLabel(detailsCursor_) + " left unchanged.";
    return true;
  }
  // Digits only (a paste arrives as one event), and few enough that the
  // number always fits in an int.
  if (event.is_character()) {
    const std::string& typed = event.character();
    bool digits = std::all_of(typed.begin(), typed.end(), [](char c) {
      return std::isdigit(static_cast<unsigned char>(c));
    });
    if (!digits || countBuffer_.size() + typed.size() > kMaxCountDigits)
      return true;
  }
  countInput_->OnEvent(event);
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

  isoDate today = ctx.today();
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
    ctx.status = "Details: [j/k] Field  [h/l] Change  [e/ret] Type a number";
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
  if (event == Event::Escape) {
    returnToCatalog();
    return true;
  }
  if (keys::isMoveDown(event) || keys::isMoveUp(event)) {
    moveDetailsCursor(keys::isMoveDown(event) ? 1 : -1);
    return true;
  }
  if (!selectedEntry())
    return false;
  if (keys::isMoveLeft(event) || keys::isMoveRight(event)) {
    adjustDetailsField(ctx, keys::isMoveRight(event) ? 1 : -1);
    return true;
  }
  bool activates =
      keys::isConfirmOrSpace(event) || keys::isCharacter(event, 'e');
  if (!activates)
    return false;
  auto field = countFieldOf(detailsCursor_);
  if (!field) {
    adjustDetailsField(ctx, 1);
    return true;
  }
  countBuffer_ = std::to_string(countOf(*selectedEntry(), *field));
  // FTXUI counts the cursor in bytes; start at the end of the number.
  countCursor_ = static_cast<int>(countBuffer_.size());
  editingCount_ = true;
  mode_ = appMode::kEdt;
  countInput_->TakeFocus();
  ctx.status = fieldLabel(detailsCursor_) + " type a number.";
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
  isoDate today = ctx.today();
  if (detailsCursor_ == detailsField::kStatus) {
    entry changed = cycleStatus(*current, direction, today);
    commit(ctx, changed, "Status: " + statusToString(changed.status, media_));
  } else if (detailsCursor_ == detailsField::kRating) {
    entry changed = adjustRating(*current, direction, today);
    commit(ctx, changed, "Rating: " + renderStarMeter(changed.score));
  } else {
    countField field = *countFieldOf(detailsCursor_);
    entry changed =
        setCount(*current, field, countOf(*current, field) + direction, today);
    commit(ctx, changed,
           fieldLabel(detailsCursor_) + " " + countLabel(changed, field));
  }
}

void catalogTab::moveDetailsCursor(int direction) {
  auto fields = detailsFields();
  auto at = std::find(fields.begin(), fields.end(), detailsCursor_);
  int index = static_cast<int>(at - fields.begin()) + direction;
  index = std::clamp(index, 0, static_cast<int>(fields.size()) - 1);
  detailsCursor_ = fields[index];
}

std::vector<detailsField> catalogTab::detailsFields() const {
  if (media_ == mediaType::kAnime)
    return {detailsField::kStatus, detailsField::kRating,
            detailsField::kProgress, detailsField::kTotal,
            detailsField::kRewatched};
  return {detailsField::kStatus,   detailsField::kRating,
          detailsField::kProgress, detailsField::kTotal,
          detailsField::kVolumes,  detailsField::kRewatched};
}

std::string catalogTab::fieldLabel(detailsField field) const {
  bool anime = media_ == mediaType::kAnime;
  switch (field) {
    case detailsField::kStatus:
      return "Status:";
    case detailsField::kRating:
      return "Rating:";
    case detailsField::kProgress:
      return "Progress:";
    case detailsField::kTotal:
      return anime ? "Episodes:" : "Chapters:";
    case detailsField::kVolumes:
      return "Volumes:";
    case detailsField::kRewatched:
      return anime ? "Rewatched:" : "Reread:";
  }
  return "";
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

void catalogTab::stopEditingCount() {
  editingCount_ = false;
  mode_ = appMode::kNor;
  idle_->TakeFocus();
}

void catalogTab::commitCount(appContext& ctx) {
  const entry* current = selectedEntry();
  auto field = countFieldOf(detailsCursor_);
  if (countBuffer_.empty()) {
    ctx.status = "Type a number, or [Esc] to cancel.";
    return;
  }
  stopEditingCount();
  if (!current || !field)
    return;
  entry changed =
      setCount(*current, *field, std::stoi(countBuffer_), ctx.today());
  commit(ctx, changed,
         fieldLabel(detailsCursor_) + " " + countLabel(changed, *field));
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
  added.updatedAt = ctx.today();
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

  std::string link = e.malId > 0 ? "MAL #" + std::to_string(e.malId) : "-";

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
          {text("  DETAILS") | bold | color(theme::kLavender), text(""),
           hbox({text("  " + shownTitle) | bold | color(theme::kMauve)}),
           text(""),
           hbox({text("  Type:     ") | color(theme::kSubtext),
                 pill(typeToIcon(e.type) + " " + typeToString(e.type),
                      theme::kCrust, typeToColor(e.type)),
                 filler()}),
           infoRow("  Link:     ", link),
           infoRow("  Updated:  ", e.updatedAt ? e.updatedAt->toString() : "-"),
           infoRow("  Started:  ",
                   e.startDate ? e.startDate->toString() : "Not started"),
           infoRow("  Finished: ", e.finishDate ? e.finishDate->toString()
                                                : unfinishedLabel)}) |
      border | color(theme::kSurface0);

  bool trackingActive = mode_ != appMode::kSch && onDetails;
  std::string percent =
      e.totalUnits > 0 ? std::to_string(static_cast<int>(ratio * 100)) + "% "
                       : "Ongoing ";
  auto countRow = [&](detailsField field) {
    countField count = *countFieldOf(field);
    bool editing = editingCount_ && onDetails && detailsCursor_ == field;
    Element value = editing
                        ? countInput_->Render() | bgcolor(theme::kSurface0) |
                              color(theme::kPeach) | size(WIDTH, EQUAL, 8)
                        : text(countLabel(e, count)) | color(theme::kText);
    return fieldRow(field, fieldLabel(field), hbox({value, filler()}));
  };
  Elements counts = {text(""),
                     text("  COUNTS") | bold | color(theme::kOverlay0),
                     countRow(detailsField::kTotal)};
  if (media_ == mediaType::kManga)
    counts.push_back(countRow(detailsField::kVolumes));
  counts.push_back(countRow(detailsField::kRewatched));

  bool editingProgress =
      editingCount_ && onDetails && detailsCursor_ == detailsField::kProgress;
  Element progressLine =
      editingProgress
          ? hbox(
                {countInput_->Render() | bgcolor(theme::kSurface0) |
                     color(theme::kPeach) | size(WIDTH, EQUAL, 8),
                 text(" / " + knownOrUnknown(e.totalUnits) + unitName(e.type)) |
                     color(theme::kText)})
          : text(progressLabel(e) + unitName(e.type)) | color(theme::kText);
  Elements tracking = {
      hbox({text("  TRACKING") | bold | color(theme::kLavender), filler(),
            text(trackingActive ? (editingCount_ ? "[Editing]" : "[Active]")
                                : "") |
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
                         color(finished ? theme::kGreen : theme::kBlue)}))};
  tracking.insert(tracking.end(), counts.begin(), counts.end());
  tracking.push_back(filler());
  Color trackingBorder = editingCount_    ? theme::kPeach
                         : trackingActive ? theme::kMauve
                                          : theme::kSurface0;
  auto trackingCard =
      vbox(std::move(tracking)) | border | color(trackingBorder) | flex;

  return vbox({seriesCard, trackingCard});
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
