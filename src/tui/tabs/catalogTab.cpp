#include "catalist/tui/tabs/catalogTab.hpp"

#include <algorithm>
#include <cctype>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/screen/terminal.hpp>

#include "catalist/core/logger.hpp"
#include "catalist/model/activity.hpp"
#include "catalist/model/entryOps.hpp"
#include "catalist/tui/actions.hpp"
#include "catalist/tui/keys.hpp"
#include "catalist/tui/theme/palettes.hpp"
#include "catalist/tui/theme/statusStyle.hpp"
#include "catalist/tui/theme/themeRegistry.hpp"
#include "catalist/tui/widgets/pill.hpp"

namespace catalist {

using namespace ftxui;

namespace {

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

bool isSortAscending(sortKey key, bool reverse) {
  switch (key) {
    case sortKey::kScore:
    case sortKey::kProgress:
    case sortKey::kDate:
      return reverse;
    case sortKey::kTitle:
    case sortKey::kStatus:
    case sortKey::kId:
      return !reverse;
  }
  return false;
}

// Numeric columns read best high-to-low first; text columns A-Z.
bool defaultsToDescending(sortKey key) {
  return key == sortKey::kScore || key == sortKey::kProgress ||
         key == sortKey::kDate;
}

std::string progressLabel(const entry& e) {
  return std::to_string(e.progress) + "/" +
         (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?");
}

std::optional<isoDate> catalogDate(const entry& e) {
  bool showsFinish = e.status == watchStatus::kCompleted && e.finishDate;
  if (showsFinish)
    return e.finishDate;
  if (e.startDate)
    return e.startDate;
  return e.updatedAt;
}

std::string formatCatalogDate(const entry& e) {
  auto shown = catalogDate(e);
  return shown ? shown->toString() : "    -     ";
}

Element styledText(const std::string& content, Color tint, bool emphasised) {
  auto element = text(content) | color(tint);
  return emphasised ? element | bold : element;
}

Element highlightMatches(const std::string& title, const std::string& query,
                         bool emphasised, const themePalette& theme) {
  std::string loweredTitle = lowercase(title);
  std::string loweredQuery = lowercase(query);
  size_t match = query.empty() ? std::string::npos : loweredTitle.find(loweredQuery);
  if (match == std::string::npos)
    return styledText(title, theme.text, emphasised);

  Elements parts;
  size_t consumed = 0;
  while (match != std::string::npos) {
    if (match > consumed)
      parts.push_back(styledText(title.substr(consumed, match - consumed),
                                 theme.text, emphasised));
    parts.push_back(text(title.substr(match, loweredQuery.size())) | bold |
                    color(theme.crust) | bgcolor(theme.yellow));
    consumed = match + loweredQuery.size();
    match = loweredTitle.find(loweredQuery, consumed);
  }
  if (consumed < title.size())
    parts.push_back(styledText(title.substr(consumed), theme.text, emphasised));
  return hbox(std::move(parts));
}

std::vector<sidebarRow> buildSidebarRows(const database& db, mediaType media) {
  auto count = [&](watchStatus status) {
    return " (" + std::to_string(db.countByStatus(media, status)) + ")";
  };
  auto scoreRow = [&](int score) {
    return sidebarRow{renderStarMeter(score) + " (" +
                          std::to_string(db.countByScore(media, score)) + ")",
                      false, std::nullopt, score};
  };
  bool isManga = media == mediaType::kManga;
  return {
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
      scoreRow(10),
      scoreRow(9),
      scoreRow(8),
      scoreRow(7),
      scoreRow(6),
      scoreRow(5),
      scoreRow(0),
  };
}

int labelIndex(const std::string& label) {
  try {
    return std::stoi(label);
  } catch (...) {
    return -1;
  }
}

template <typename member>
void toggleMembership(std::set<member>& members, const member& item) {
  if (members.count(item))
    members.erase(item);
  else
    members.insert(item);
}

}  // namespace

catalogTab::catalogTab(mediaType media, const appConfig& config)
    : media_(media), reverseSort_(config.reverseSort) {
  buildComponents();
}

std::string catalogTab::id() const {
  return media_ == mediaType::kAnime ? "anime" : "manga";
}

std::string catalogTab::title() const {
  return media_ == mediaType::kAnime ? "Anime" : "Manga";
}

std::string catalogTab::icon() const {
  return media_ == mediaType::kAnime ? icon::kAnime : icon::kManga;
}

void catalogTab::buildComponents() {
  MenuOption sidebarOption;
  sidebarOption.entries_option.transform = [this](const EntryState& state) {
    return renderSidebarRow(labelIndex(state.label));
  };
  sidebarMenu_ = Menu(&sidebarLabels_, &sidebarSelected_, sidebarOption);

  InputOption inputOption;
  inputOption.transform = [](InputState state) {
    const auto& theme = themeRegistry::active();
    Element field = state.element | color(state.is_placeholder ? theme.overlay0
                                                               : theme.text);
    return state.focused ? field | color(theme.peach) : field;
  };
  searchInput_ = Input(&searchQuery_, "Press '/' to search...", inputOption);
  remoteInput_ =
      Input(&remoteQuery_, "Search title and press [ret]...", inputOption);

  MenuOption catalogOption;
  catalogOption.entries_option.transform = [this](const EntryState& state) {
    return renderCatalogRow(labelIndex(state.label));
  };
  catalogMenu_ = Menu(&catalogLabels_, &catalogSelected_, catalogOption);

  MenuOption remoteOption;
  remoteOption.entries_option.transform = [this](const EntryState& state) {
    return renderRemoteRow(labelIndex(state.label));
  };
  remoteMenu_ = Menu(&remoteLabels_, &remoteSelected_, remoteOption);
  notesInput_ = Input(&notesBuffer_, "Write notes here...");

  root_ = Container::Horizontal(
      {sidebarMenu_, catalogMenu_, notesInput_,
       Container::Vertical({remoteInput_, remoteMenu_})});
}

void catalogTab::onActivate(appContext& ctx) {
  refresh(ctx);
}

bool catalogTab::isRowFilterActive(int row) const {
  if (row < 0 || row >= static_cast<int>(sidebarRows_.size()))
    return false;
  const auto& candidate = sidebarRows_[row];
  if (candidate.status)
    return statusFilters_.count(*candidate.status) > 0;
  if (candidate.score)
    return scoreFilters_.count(*candidate.score) > 0;
  return false;
}

bool catalogTab::passesFilters(const entry& candidate) const {
  if (!statusFilters_.empty() && !statusFilters_.count(candidate.status))
    return false;
  if (!scoreFilters_.empty() && !scoreFilters_.count(candidate.score))
    return false;

  bool usesSidebarRow = statusFilters_.empty() && scoreFilters_.empty() &&
                        sidebarSelected_ >= 0 &&
                        sidebarSelected_ < static_cast<int>(sidebarRows_.size());
  if (usesSidebarRow) {
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
  sidebarLabels_.clear();
  for (size_t i = 0; i < sidebarRows_.size(); ++i)
    sidebarLabels_.push_back(std::to_string(i));

  displayed_.clear();
  for (const auto& candidate : ctx.db.getEntries(media_)) {
    if (passesFilters(candidate))
      displayed_.push_back(candidate);
  }
  database::sortEntries(displayed_, sortBy_, reverseSort_);

  catalogLabels_.clear();
  for (size_t i = 0; i < displayed_.size(); ++i)
    catalogLabels_.push_back(std::to_string(i));
  int lastRow = static_cast<int>(displayed_.size()) - 1;
  catalogSelected_ = std::clamp(catalogSelected_, 0, std::max(0, lastRow));
}

const entry* catalogTab::selectedEntry() const {
  bool inRange = catalogSelected_ >= 0 &&
                 catalogSelected_ < static_cast<int>(displayed_.size());
  return inRange ? &displayed_[catalogSelected_] : nullptr;
}

bool catalogTab::capturesInput() const {
  return pendingDeleteId_.has_value() || remoteDialogOpen_ ||
         mode_ == appMode::kSch || editingNotes_;
}

std::optional<std::string> catalogTab::countBadge(appContext& ctx) const {
  return std::to_string(ctx.db.getEntries(media_).size());
}

std::string catalogTab::selectionSummary() const {
  if (visualSelectedIds_.empty())
    return "";
  return "(" + std::to_string(visualSelectedIds_.size()) + " selected)";
}

std::vector<hint> catalogTab::hints() const {
  if (editingNotes_)
    return {{"ret", "commit"}, {"Esc", "cancel"}};
  if (mode_ == appMode::kSch)
    return {{"ret", "apply"}, {"Esc", "clear"}};
  if (focus_ == focusedCard::kDetails)
    return {{"j/k", "field"},
            {"h/l", "adjust"},
            {"e/ret", "notes"},
            {"Esc", "catalog"}};
  return {{"Tab", "tabs"},       {"h/l", "panes"},  {"e/ret", "edit"},
          {"c/t/s/r/p", "sort"}, {"S/R/P", "cycle"}, {"+/-", "ep"},
          {"spc", "select"},     {"a", "add"},       {"d", "del"},
          {"/", "find"},         {":", "cmd"}};
}

// --- events -----------------------------------------------------------------

bool catalogTab::onEvent(const Event& event, appContext& ctx) {
  if (handleMouse(event, ctx))
    return true;
  if (pendingDeleteId_)
    return handlePendingDelete(event, ctx);
  if (remoteDialogOpen_)
    return handleRemoteDialog(event, ctx);
  if (mode_ == appMode::kSch)
    return handleSearchTyping(event, ctx);
  if (editingNotes_)
    return handleNotesEditing(event, ctx);
  if (handleBatchKeys(event, ctx))
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
  return handledByFocus || handleEntryCommands(event, ctx);
}

bool catalogTab::handleMouse(const Event& event, appContext& ctx) {
  if (!event.is_mouse())
    return false;
  Event mouseEvent = event;
  auto mouse = mouseEvent.mouse();
  if (mouse.button != Mouse::Left || mouse.motion != Mouse::Pressed)
    return false;

  if (searchBarBox_.Contain(mouse.x, mouse.y)) {
    startSearch(ctx);
    return true;
  }
  const std::pair<Box*, std::pair<sortKey, const char*>> headers[] = {
      {&headerDateBox_, {sortKey::kDate, "Date"}},
      {&headerTitleBox_, {sortKey::kTitle, "Title"}},
      {&headerStatusBox_, {sortKey::kStatus, "Status"}},
      {&headerRatingBox_, {sortKey::kScore, "Rating"}},
      {&headerProgressBox_, {sortKey::kProgress, "Progress"}}};
  for (const auto& [box, column] : headers) {
    if (!box->Contain(mouse.x, mouse.y))
      continue;
    toggleSort(column.first, column.second, ctx);
    return true;
  }
  return false;
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
    ctx.notices.setStatus("Deletion canceled.");
  }
  return true;
}

bool catalogTab::handleRemoteDialog(const Event& event, appContext& ctx) {
  if (event == Event::Escape) {
    remoteDialogOpen_ = false;
    mode_ = appMode::kNor;
    catalogMenu_->TakeFocus();
    return true;
  }
  if (event == Event::Tab) {
    if (remoteInput_->Focused())
      remoteMenu_->TakeFocus();
    else
      remoteInput_->TakeFocus();
    return true;
  }
  if (!remoteInput_->Focused())
    return handleRemoteResults(event, ctx);

  if (keys::isConfirm(event)) {
    runRemoteSearch(ctx);
    remoteMenu_->TakeFocus();
    return true;
  }
  if (event == Event::ArrowDown) {
    remoteMenu_->TakeFocus();
    return true;
  }
  remoteInput_->OnEvent(event);
  return true;
}

bool catalogTab::handleRemoteResults(const Event& event, appContext& ctx) {
  if (!remoteMenu_->Focused())
    return true;
  int lastResult = static_cast<int>(remoteResults_.size()) - 1;
  if (keys::isConfirm(event) && remoteSelected_ >= 0 &&
      remoteSelected_ <= lastResult) {
    addRemoteSelection(ctx);
    return true;
  }
  if (keys::isMoveUp(event, ctx.config.keybinds)) {
    if (remoteSelected_ > 0)
      remoteSelected_--;
    else
      remoteInput_->TakeFocus();
    return true;
  }
  if (keys::isMoveDown(event, ctx.config.keybinds) && remoteSelected_ < lastResult)
    remoteSelected_++;
  return true;
}

bool catalogTab::handleSearchTyping(const Event& event, appContext& ctx) {
  if (event == Event::Escape) {
    searchQuery_.clear();
    refresh(ctx);
    focus_ = focusedCard::kCatalog;
    catalogMenu_->TakeFocus();
    mode_ = appMode::kNor;
    ctx.notices.setStatus("Search cleared.");
    return true;
  }
  if (keys::isConfirm(event) || event == Event::ArrowDown) {
    refresh(ctx);
    focus_ = focusedCard::kCatalog;
    catalogMenu_->TakeFocus();
    mode_ = appMode::kNor;
    ctx.notices.setStatus(
        searchQuery_.empty()
            ? "Filter cleared."
            : "Filtered: '" + searchQuery_ + "' (" +
                  std::to_string(displayed_.size()) + " matches)");
    return true;
  }
  searchInput_->OnEvent(event);
  refresh(ctx);
  return true;
}

bool catalogTab::handleNotesEditing(const Event& event, appContext& ctx) {
  if (event == Event::Escape || keys::isConfirm(event)) {
    commitNotes(ctx);
    return true;
  }
  notesInput_->OnEvent(event);
  return true;
}

bool catalogTab::handleBatchKeys(const Event& event, appContext& ctx) {
  if (keys::isCharacter(event, 'S')) {
    applyToTargets(ctx, [](const entry& e, const isoDate& today) {
      return cycleStatus(e, 1, today);
    }, "cycled status");
    return true;
  }
  if (keys::isCharacter(event, 'R')) {
    applyToTargets(ctx, [](const entry& e, const isoDate& today) {
      return adjustRating(e, 1, today);
    }, "bumped rating");
    return true;
  }
  bool increments = keys::isCharacter(event, 'P') ||
                    keys::isBound(event, ctx.config.keybinds,
                                  keyAction::kIncrementProgress);
  if (increments) {
    applyToTargets(ctx, [](const entry& e, const isoDate& today) {
      return stepProgress(e, 1, today);
    }, "incremented progress");
    return true;
  }
  if (keys::isBound(event, ctx.config.keybinds, keyAction::kDecrementProgress)) {
    applyToTargets(ctx, [](const entry& e, const isoDate& today) {
      return stepProgress(e, -1, today);
    }, "decremented progress");
    return true;
  }
  return false;
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
  const auto& keybinds = ctx.config.keybinds;
  int lastRow = static_cast<int>(displayed_.size()) - 1;
  if (keys::isMoveLeft(event, keybinds)) {
    focus_ = focusedCard::kSidebar;
    sidebarMenu_->TakeFocus();
    return true;
  }
  if (keys::isMoveRight(event, keybinds)) {
    focus_ = focusedCard::kDetails;
    detailsCursor_ = detailsField::kStatus;
    return true;
  }
  if (keys::isCharacter(event, ' ') || keys::isCharacter(event, 'v')) {
    toggleVisualSelection();
    return true;
  }
  if (keys::isConfirm(event) || keys::isCharacter(event, 'e')) {
    focus_ = focusedCard::kDetails;
    detailsCursor_ = detailsField::kStatus;
    ctx.notices.setStatus(
        "Details: [j/k] Navigate properties, [h/l] Adjust, [e/ret] Notes");
    return true;
  }
  if (keys::isMoveDown(event, keybinds)) {
    catalogSelected_ = std::min(catalogSelected_ + 1, std::max(0, lastRow));
    return true;
  }
  if (keys::isMoveUp(event, keybinds)) {
    catalogSelected_ = std::max(catalogSelected_ - 1, 0);
    return true;
  }
  if (keys::isJumpTop(event, keybinds)) {
    catalogSelected_ = 0;
    return true;
  }
  if (keys::isJumpBottom(event, keybinds)) {
    catalogSelected_ = std::max(0, lastRow);
    return true;
  }
  return false;
}

bool catalogTab::handleSidebarFocus(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  if (keys::isMoveRight(event, keybinds)) {
    focus_ = focusedCard::kCatalog;
    catalogMenu_->TakeFocus();
    return true;
  }
  if (keys::isMoveDown(event, keybinds)) {
    moveSidebarCursor(1, ctx);
    return true;
  }
  if (keys::isMoveUp(event, keybinds)) {
    moveSidebarCursor(-1, ctx);
    return true;
  }
  if (keys::isCharacter(event, ' ')) {
    toggleSidebarMultiFilter();
    refresh(ctx);
    return true;
  }
  if (keys::isConfirm(event)) {
    statusFilters_.clear();
    scoreFilters_.clear();
    refresh(ctx);
    focus_ = focusedCard::kCatalog;
    catalogMenu_->TakeFocus();
    return true;
  }
  return false;
}

bool catalogTab::handleDetailsFocus(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  int cursor = static_cast<int>(detailsCursor_);
  if (event == Event::Escape) {
    focus_ = focusedCard::kCatalog;
    mode_ = appMode::kNor;
    return true;
  }
  if (keys::isMoveDown(event, keybinds)) {
    detailsCursor_ = static_cast<detailsField>(std::min(cursor + 1, 3));
    return true;
  }
  if (keys::isMoveUp(event, keybinds)) {
    detailsCursor_ = static_cast<detailsField>(std::max(cursor - 1, 0));
    return true;
  }
  if (!selectedEntry())
    return false;

  if (keys::isMoveLeft(event, keybinds)) {
    if (detailsCursor_ == detailsField::kNotes)
      focus_ = focusedCard::kCatalog;
    else
      adjustDetailsField(ctx, -1);
    return true;
  }
  if (keys::isMoveRight(event, keybinds)) {
    if (detailsCursor_ != detailsField::kNotes)
      adjustDetailsField(ctx, 1);
    return true;
  }
  bool activates = keys::isConfirmOrSpace(event) || keys::isCharacter(event, 'e');
  if (!activates)
    return false;
  if (detailsCursor_ != detailsField::kNotes) {
    adjustDetailsField(ctx, 1);
    return true;
  }
  notesBuffer_ = selectedEntry()->notes;
  editingNotes_ = true;
  mode_ = appMode::kEdt;
  notesInput_->TakeFocus();
  ctx.notices.setStatus("Editing notes. Press [ret] or [Esc] to commit.");
  return true;
}

bool catalogTab::handleEntryCommands(const Event& event, appContext& ctx) {
  const auto& keybinds = ctx.config.keybinds;
  if (keys::isBound(event, keybinds, keyAction::kSearchFilter)) {
    startSearch(ctx);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kAddEntry)) {
    openRemoteDialog();
    remoteInput_->TakeFocus();
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kDeleteEntry)) {
    requestDelete(ctx);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kUndo)) {
    undoLast(ctx);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kRedo) ||
      event == Event::Special("\x12")) {
    redoLast(ctx);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kOpenBrowser)) {
    if (const entry* current = selectedEntry())
      openInBrowser(ctx, *current);
    return true;
  }
  if (keys::isBound(event, keybinds, keyAction::kYankTitle)) {
    if (const entry* current = selectedEntry())
      copyToClipboard(ctx, current->title);
    return true;
  }
  return false;
}

// --- actions ----------------------------------------------------------------

void catalogTab::applyChange(appContext& ctx, const entry& before,
                             const entryChange& change,
                             const std::string& status) {
  entry after = change(before, localDate(ctx.clock));
  ctx.undo.record({historyAction::kMutate, after.type, before, after});
  ctx.db.updateEntry(after);
  ctx.db.logActivity({activityTimestamp(ctx.clock), after.type, after.title,
                      after.status, after.progress, after.totalUnits,
                      after.score});
  ctx.db.saveAll();
  refresh(ctx);
  if (!status.empty())
    ctx.notices.setStatus(status);
}

void catalogTab::applyToTargets(appContext& ctx, const entryChange& change,
                                const std::string& batchVerb) {
  if (visualSelectedIds_.empty()) {
    const entry* current = selectedEntry();
    if (current)
      applyChange(ctx, *current, change, "");
    return;
  }
  for (int targetId : visualSelectedIds_) {
    if (auto target = ctx.db.getById(media_, targetId))
      applyChange(ctx, *target, change, "");
  }
  ctx.notices.setStatus("Batch " + batchVerb + " for " +
                        std::to_string(visualSelectedIds_.size()) +
                        " entries.");
}

void catalogTab::adjustDetailsField(appContext& ctx, int direction) {
  const entry* current = selectedEntry();
  if (!current)
    return;
  entry before = *current;
  if (detailsCursor_ == detailsField::kStatus) {
    LOG_INFO("Cycling status for ID " + std::to_string(before.id));
    entry after = cycleStatus(before, direction, localDate(ctx.clock));
    applyChange(ctx, before, [&](const entry&, const isoDate&) { return after; },
                "Status: " + statusToString(after.status, after.type));
    return;
  }
  if (detailsCursor_ == detailsField::kRating) {
    LOG_INFO("Adjusting rating for ID " + std::to_string(before.id));
    entry after = adjustRating(before, direction, localDate(ctx.clock));
    applyChange(ctx, before, [&](const entry&, const isoDate&) { return after; },
                "Rating: " + renderStarMeter(after.score));
    return;
  }
  LOG_INFO("Mutating progress for ID " + std::to_string(before.id));
  entry after = stepProgress(before, direction, localDate(ctx.clock));
  applyChange(ctx, before, [&](const entry&, const isoDate&) { return after; },
              "Progress: " + progressLabel(after));
}

void catalogTab::toggleSort(sortKey key, const std::string& name,
                            appContext& ctx) {
  LOG_INFO("Toggled sort column: " + name);
  if (sortBy_ == key) {
    reverseSort_ = !reverseSort_;
  } else {
    sortBy_ = key;
    reverseSort_ = defaultsToDescending(key);
  }
  refresh(ctx);
  bool ascending = isSortAscending(sortBy_, reverseSort_);
  ctx.notices.setStatus("Sort: " + name +
                        (ascending ? " (Increasing ▲)" : " (Decreasing ▼)"));
}

void catalogTab::toggleVisualSelection() {
  const entry* current = selectedEntry();
  if (!current)
    return;
  toggleMembership(visualSelectedIds_, current->id);
  mode_ = visualSelectedIds_.empty() ? appMode::kNor : appMode::kVis;
}

void catalogTab::toggleSidebarMultiFilter() {
  if (sidebarSelected_ < 0 ||
      sidebarSelected_ >= static_cast<int>(sidebarRows_.size()))
    return;
  const auto& row = sidebarRows_[sidebarSelected_];
  if (row.status)
    toggleMembership(statusFilters_, *row.status);
  if (row.score)
    toggleMembership(scoreFilters_, *row.score);
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
  ctx.notices.setStatus("Search title... [ret] Apply  [Esc] Clear");
}

void catalogTab::openRemoteDialog() {
  remoteDialogOpen_ = true;
  mode_ = appMode::kSch;
  remoteQuery_ = searchQuery_;
  remoteResults_.clear();
  remoteLabels_.clear();
  remoteSelected_ = 0;
  remoteStatus_.clear();
}

void catalogTab::runRemoteSearch(appContext& ctx) {
  if (remoteQuery_.empty())
    return;
  std::string err;
  remoteResults_ =
      remoteSearchEngine(ctx.http).query(media_, remoteQuery_, err, 10);
  remoteLabels_.clear();
  for (size_t i = 0; i < remoteResults_.size(); ++i)
    remoteLabels_.push_back(std::to_string(i));
  remoteSelected_ = 0;
  if (remoteResults_.empty())
    remoteStatus_ = err.empty() ? "No results found." : err;
  else
    remoteStatus_ =
        std::to_string(remoteResults_.size()) + " matches from MyAnimeList.";
}

void catalogTab::addRemoteSelection(appContext& ctx) {
  const auto& picked = remoteResults_[remoteSelected_];
  isoDate today = localDate(ctx.clock);
  entry added;
  added.id = picked.malId;
  added.malId = picked.malId;
  added.type = picked.type;
  added.title = picked.title;
  added.status = watchStatus::kWatching;
  added.totalUnits = picked.totalUnits;
  added.startDate = isoDate::parse(picked.year + "-01-01").value_or(today);
  added.updatedAt = today;
  ctx.db.addEntry(added);
  ctx.undo.record({historyAction::kAdd, added.type, entry{}, added});
  ctx.db.saveAll();
  refresh(ctx);

  auto shown = std::find_if(displayed_.begin(), displayed_.end(),
                            [&](const entry& e) { return e.id == added.id; });
  if (shown != displayed_.end())
    catalogSelected_ = static_cast<int>(shown - displayed_.begin());
  remoteDialogOpen_ = false;
  mode_ = appMode::kNor;
  catalogMenu_->TakeFocus();
  ctx.notices.push("Successfully added '" + added.title + "'.");
}

void catalogTab::commitNotes(appContext& ctx) {
  const entry* current = selectedEntry();
  if (current) {
    entry before = *current;
    entry after = before;
    after.notes = notesBuffer_;
    after.updatedAt = localDate(ctx.clock);
    ctx.undo.record({historyAction::kMutate, after.type, before, after});
    ctx.db.updateEntry(after);
    ctx.db.saveAll();
    refresh(ctx);
  }
  editingNotes_ = false;
  mode_ = appMode::kNor;
  ctx.notices.setStatus("Saved notes.");
}

void catalogTab::requestDelete(appContext& ctx) {
  if (!visualSelectedIds_.empty()) {
    for (int selectedId : visualSelectedIds_)
      ctx.db.removeEntry(media_, selectedId);
    visualSelectedIds_.clear();
    mode_ = appMode::kNor;
    ctx.db.saveAll();
    refresh(ctx);
    ctx.notices.push("Deleted selected entries.");
    return;
  }
  const entry* current = selectedEntry();
  if (!current)
    return;
  pendingDeleteId_ = current->id;
  ctx.notices.setStatus("Delete '" + marquee::clip(current->title, 24) +
                        "'? Confirm [y/N]");
}

void catalogTab::confirmDelete(appContext& ctx) {
  auto doomed = ctx.db.getById(media_, *pendingDeleteId_);
  pendingDeleteId_.reset();
  if (doomed) {
    ctx.undo.record({historyAction::kDelete, media_, *doomed, entry{}});
    ctx.db.removeEntry(media_, doomed->id);
    ctx.db.saveAll();
    ctx.notices.push("Deleted '" + marquee::clip(doomed->title, 36) + "'.");
  }
  refresh(ctx);
}

void catalogTab::undoLast(appContext& ctx) {
  auto action = ctx.undo.takeUndo();
  if (!action) {
    ctx.notices.push("Nothing left to undo.");
    return;
  }
  if (action->actionType == historyAction::kMutate)
    ctx.db.updateEntry(action->before);
  else if (action->actionType == historyAction::kDelete)
    ctx.db.addEntry(action->before);
  else
    ctx.db.removeEntry(action->media, action->after.id);
  ctx.db.saveAll();
  ctx.notices.push("Action undone successfully.");
  refresh(ctx);
}

void catalogTab::redoLast(appContext& ctx) {
  auto action = ctx.undo.takeRedo();
  if (!action) {
    ctx.notices.push("Nothing left to redo.");
    return;
  }
  if (action->actionType == historyAction::kMutate)
    ctx.db.updateEntry(action->after);
  else if (action->actionType == historyAction::kDelete)
    ctx.db.removeEntry(action->media, action->before.id);
  else
    ctx.db.addEntry(action->after);
  ctx.db.saveAll();
  ctx.notices.push("Action redone successfully.");
  refresh(ctx);
}

// --- rendering --------------------------------------------------------------

Element catalogTab::renderSidebarRow(int row) const {
  const auto& theme = themeRegistry::active();
  if (row < 0 || row >= static_cast<int>(sidebarRows_.size()))
    return text("");
  const auto& item = sidebarRows_[row];
  if (item.isHeader)
    return text(item.label) | bold | color(theme.overlay0);

  bool usesMultiFilter = !statusFilters_.empty() || !scoreFilters_.empty();
  bool inMultiFilter = isRowFilterActive(row);
  bool isSingleFilter = !usesMultiFilter && row == sidebarSelected_;
  bool cursorHere = mode_ != appMode::kSch && row == sidebarSelected_ &&
                    focus_ == focusedCard::kSidebar;
  std::string prefix = inMultiFilter    ? icon::kCheckMulti + " "
                       : isSingleFilter ? icon::kCheckSingle + " "
                                        : "   ";

  if (cursorHere)
    return hbox({text(prefix) | color(theme.mauve) | bold,
                 text(item.label) | bold | color(theme.text)}) |
           bgcolor(theme.surface0) | focus;
  if (inMultiFilter || isSingleFilter)
    return hbox({text(prefix) | color(theme.green) | bold,
                 text(item.label) | bold | color(theme.lavender)});
  return hbox({text("   "), text(item.label) | color(theme.subtext)});
}

Element catalogTab::renderCatalogRow(int row) {
  const auto& theme = themeRegistry::active();
  if (row < 0 || row >= static_cast<int>(displayed_.size()))
    return text("");
  const auto& e = displayed_[row];

  bool rowActive = mode_ != appMode::kSch && row == catalogSelected_ &&
                   focus_ != focusedCard::kSidebar;
  bool selected = visualSelectedIds_.count(e.id) > 0;
  size_t maxGlyphs =
      static_cast<size_t>(std::max(16, Terminal::Size().dimx - 134));
  std::string shownTitle =
      rowActive ? marquee::scroll(e.title, maxGlyphs,
                                  rowTimer_.elapsedFor(catalogSelected_))
                : marquee::clip(e.title, maxGlyphs);

  std::string marker = "  ";
  Color markerColor = theme.green;
  if (selected) {
    marker = (visualSelectedIds_.size() > 1 ? icon::kCheckMulti
                                            : icon::kCheckSingle) +
             " ";
  } else if (rowActive) {
    marker = "▌ ";
    markerColor = theme.mauve;
  }

  auto statusCell =
      hbox({text(statusToIcon(e.status) + " ") | color(statusToColor(e.status)),
            text(statusToString(e.status, e.type)) |
                color(statusToColor(e.status))});
  auto line = hbox(
      {text(marker) | bold | color(markerColor),
       text(formatCatalogDate(e)) | size(WIDTH, EQUAL, 10) |
           color(theme.subtext),
       text(" "),
       hbox({text(" " + typeToIcon(e.type) + " "),
             highlightMatches(shownTitle, searchQuery_, rowActive, theme)}) |
           flex,
       text(" "), statusCell | size(WIDTH, EQUAL, 14), text(" "),
       text(" " + renderStarMeter(e.score)) | size(WIDTH, EQUAL, 12) |
           color(theme.yellow),
       text(" "),
       text(" " + icon::kGauge + " " + progressLabel(e)) |
           size(WIDTH, EQUAL, 12) | color(theme.lavender),
       text(" ")});

  if (rowActive)
    return line | bgcolor(theme.surface0) | focus;
  return selected ? line | bgcolor(theme.surface0) : line;
}

Element catalogTab::renderRemoteRow(int row) const {
  const auto& theme = themeRegistry::active();
  if (row < 0 || row >= static_cast<int>(remoteResults_.size()))
    return text("");
  const auto& result = remoteResults_[row];
  bool active = row == remoteSelected_;
  std::string score = result.score > 0.0
                          ? std::to_string(result.score).substr(0, 4)
                          : " -  ";

  auto line = hbox(
      {text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
       text("[" + std::to_string(row + 1) + "] ") | size(WIDTH, EQUAL, 5) |
           color(theme.peach) | bold,
       text(marquee::clip(result.title, 34)) | size(WIDTH, EQUAL, 34) |
           color(theme.text) | (active ? bold : nothing),
       text(" "),
       text(result.year.empty() ? " -  " : result.year) |
           size(WIDTH, EQUAL, 6) | color(theme.subtext),
       text(" "),
       text(result.totalUnits > 0 ? std::to_string(result.totalUnits) + " eps"
                                  : "ongoing") |
           size(WIDTH, EQUAL, 9) | color(theme.subtext),
       text(" "),
       text("★ " + score) | size(WIDTH, EQUAL, 8) | color(theme.yellow),
       text(" "),
       text("MAL " + std::to_string(result.malId)) | size(WIDTH, EQUAL, 10) |
           color(theme.overlay0)});
  return active ? line | bgcolor(theme.surface0) | focus : line;
}

Element catalogTab::renderHeaderCell(const std::string& name, sortKey key,
                                     int width, Box& box,
                                     bool stretches) const {
  const auto& theme = themeRegistry::active();
  bool active = sortBy_ == key;
  std::string label = " " + name;
  if (active)
    label += isSortAscending(key, reverseSort_) ? " ▲" : " ▼";
  Element cell = text(label);
  cell = stretches ? cell | flex : cell | size(WIDTH, EQUAL, width);
  cell = active ? cell | bold | color(theme.mauve) | underlined
                : cell | bold | color(theme.subtext);
  return cell | reflect(box);
}

Element catalogTab::renderDetails(appContext& ctx) {
  const auto& theme = themeRegistry::active();
  const entry* current = selectedEntry();
  if (!current)
    return text("No record selected.") | color(theme.subtext);
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
    return hbox({text(active ? "▌ " : "  ") | color(theme.mauve) | bold,
                 text(label) | size(WIDTH, EQUAL, 12) |
                     (active ? bold : nothing) |
                     color(active ? theme.peach : theme.subtext),
                 content | flex});
  };

  Elements providerBadges;
  for (const auto& provider : ctx.providers.getAllProviders()) {
    std::string remoteId = e.getRemoteId(provider->getKey());
    if (remoteId.empty())
      continue;
    if (!providerBadges.empty())
      providerBadges.push_back(text("  "));
    providerBadges.push_back(
        text(provider->getShortBadge() + " #" + remoteId) | color(theme.text));
  }
  if (providerBadges.empty())
    providerBadges.push_back(text("-") | color(theme.subtext));

  std::string shownTitle =
      marquee::scroll(e.title, 36, detailsTimer_.elapsedFor(catalogSelected_));
  std::string unfinishedLabel =
      e.status == watchStatus::kCompleted ? "Completed" : "In progress";
  auto infoRow = [&](const std::string& label, const std::string& value) {
    return hbox({text(label) | color(theme.subtext),
                 text(value) | color(theme.text), filler()});
  };

  auto seriesCard =
      vbox({hbox({text("  DETAILS") | bold | color(theme.lavender), filler(),
                  text("ID #" + std::to_string(e.id) + " ") |
                      color(theme.overlay0)}),
            text(""),
            hbox({text("  " + shownTitle) | bold | color(theme.mauve)}),
            text(""),
            hbox({text("  Type:     ") | color(theme.subtext),
                  pill(typeToIcon(e.type) + " " + typeToString(e.type),
                       theme.crust, typeToColor(e.type)),
                  filler()}),
            hbox({text("  Links:    ") | color(theme.subtext),
                  hbox(std::move(providerBadges)) | flex}),
            infoRow("  Updated:  ", e.updatedAt ? e.updatedAt->toString() : "-"),
            infoRow("  Started:  ",
                    e.startDate ? e.startDate->toString() : "Not started"),
            infoRow("  Finished: ", e.finishDate ? e.finishDate->toString()
                                                 : unfinishedLabel)}) |
      border | color(theme.surface0);

  bool trackingActive = mode_ != appMode::kSch && onDetails &&
                        detailsCursor_ != detailsField::kNotes;
  std::string percent = e.totalUnits > 0
                            ? std::to_string(static_cast<int>(ratio * 100)) + "% "
                            : "Ongoing ";
  std::string units = e.type == mediaType::kAnime ? " eps" : " chs";
  auto trackingCard =
      vbox({hbox({text("  TRACKING") | bold | color(theme.lavender), filler(),
                  text(trackingActive ? "[Active]" : "") | color(theme.peach)}),
            text(""), text("  STATUS") | bold | color(theme.overlay0),
            fieldRow(detailsField::kStatus, "Status:",
                     pill(statusToIcon(e.status) + " " +
                              statusToString(e.status, e.type),
                          theme.crust, statusToColor(e.status))),
            text(""), text("  METRICS") | bold | color(theme.overlay0),
            fieldRow(detailsField::kRating, "Rating:",
                     hbox({text(renderStarMeter(e.score)) | color(theme.yellow),
                           text(" (" + std::to_string(e.score) + "/10)") |
                               color(theme.text)})),
            text(""),
            fieldRow(detailsField::kProgress, "Progress:",
                     vbox({hbox({text(std::to_string(e.progress) + " / " +
                                      (e.totalUnits > 0
                                           ? std::to_string(e.totalUnits)
                                           : "?") +
                                      units) |
                                     color(theme.text),
                                 filler(), text(percent) | color(theme.subtext)}),
                           gauge(ratio) |
                               color(finished ? theme.green : theme.blue)}))}) |
      border | color(trackingActive ? theme.mauve : theme.surface0);

  bool notesActive = mode_ != appMode::kSch && onDetails &&
                     detailsCursor_ == detailsField::kNotes;
  Element notesBody =
      editingNotes_
          ? notesInput_->Render() | border | color(theme.peach)
          : text(e.notes.empty() ? "(empty - press e or ret to edit)" : e.notes) |
                color(e.notes.empty() ? theme.overlay0 : theme.subtext);
  std::string notesBadge =
      notesActive ? (editingNotes_ ? "[Editing]" : "[Active]") : "";
  Color notesBorder = notesActive
                          ? (editingNotes_ ? theme.peach : theme.mauve)
                          : theme.surface0;
  auto notesCard =
      vbox({hbox({text("  NOTES") | bold | color(theme.lavender), filler(),
                  text(notesBadge) | color(theme.peach)}),
            text(""), fieldRow(detailsField::kNotes, "Notes:", notesBody),
            filler()}) |
      border | color(notesBorder) | flex;

  return vbox({seriesCard, trackingCard, notesCard});
}

Element catalogTab::renderSidebarCard() const {
  const auto& theme = themeRegistry::active();
  bool focused = mode_ != appMode::kSch && focus_ == focusedCard::kSidebar;
  return vbox({text("  Categories") | bold | color(theme.lavender), text(""),
               sidebarMenu_->Render() | vscroll_indicator | frame | flex}) |
         border | color(focused ? theme.mauve : theme.surface0) |
         size(WIDTH, EQUAL, 28);
}

Element catalogTab::renderSearchCard() {
  const auto& theme = themeRegistry::active();
  std::string summary = searchQuery_.empty()
                            ? "All entries "
                            : "Filter: '" + searchQuery_ + "' ";
  return hbox({text(" " + icon::kSearch + " ") | color(theme.peach),
               searchInput_->Render() | flex,
               text(summary) | color(theme.subtext), text(" ")}) |
         border |
         color(mode_ == appMode::kSch ? theme.peach : theme.surface0) |
         size(HEIGHT, EQUAL, 3) | reflect(searchBarBox_);
}

Element catalogTab::renderCatalogCard() {
  const auto& theme = themeRegistry::active();
  auto header = hbox(
      {text("   ") | size(WIDTH, EQUAL, 3),
       renderHeaderCell("Date", sortKey::kDate, 10, headerDateBox_, false),
       text(" "),
       renderHeaderCell("Title", sortKey::kTitle, 28, headerTitleBox_, true),
       text(" "),
       renderHeaderCell("Status", sortKey::kStatus, 14, headerStatusBox_, false),
       text(" "),
       renderHeaderCell("Rating", sortKey::kScore, 12, headerRatingBox_, false),
       text(" "),
       renderHeaderCell("Progress", sortKey::kProgress, 12, headerProgressBox_,
                        false),
       text(" ")});
  Element rows = displayed_.empty()
                     ? text(" No matching entries found.") |
                           color(theme.subtext) | center | flex
                     : catalogMenu_->Render() | vscroll_indicator | frame | flex;
  bool focused = mode_ != appMode::kSch && focus_ == focusedCard::kCatalog;
  return vbox({header, text(""), rows}) | border |
         color(focused ? theme.mauve : theme.surface0) | flex;
}

Element catalogTab::render(appContext& ctx) {
  return hbox({renderSidebarCard(),
               vbox({renderSearchCard(), renderCatalogCard() | flex}) | flex,
               vbox({renderDetails(ctx) | flex}) | size(WIDTH, EQUAL, 42)});
}

std::optional<Element> catalogTab::overlay(appContext&) {
  if (!remoteDialogOpen_)
    return std::nullopt;
  const auto& theme = themeRegistry::active();
  Element results =
      text(" Type query above and press [ret] to query MyAnimeList.") |
      color(theme.subtext) | center | flex;
  if (!remoteResults_.empty())
    results = remoteMenu_->Render() | vscroll_indicator | frame | flex;
  else if (!remoteStatus_.empty())
    results = text(" " + remoteStatus_) | color(theme.red) | center | flex;

  return vbox({hbox({text("  󰖟 Online Search & Add ") | bold |
                         color(theme.mauve),
                     filler(), text(" [Esc] Close ") | color(theme.subtext)}),
               text(""),
               remoteInput_->Render() | border |
                   color(remoteInput_->Focused() ? theme.peach
                                                 : theme.surface0),
               text(""),
               vbox({results}) | border |
                   color(remoteMenu_->Focused() ? theme.mauve : theme.surface0) |
                   size(HEIGHT, EQUAL, 14),
               text(""),
               text(" [ret] Add/Search  │  [Tab] Switch Input/Results  │  "
                    "[j/k] Select  │  [Esc] Close ") |
                   color(theme.subtext)}) |
         border | color(theme.mauve) | bgcolor(theme.base) |
         size(WIDTH, EQUAL, 86) | clear_under | center;
}

}  // namespace catalist
