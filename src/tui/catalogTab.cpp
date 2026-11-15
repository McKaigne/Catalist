#include "tui/catalogTab.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>

#include "anilist.hpp"
#include "entry.hpp"
#include "text.hpp"

namespace catalist {

using ftxui::Event;

namespace {

constexpr size_t kMaxNumberDigits = 6;

std::string trimmed(const std::string& text) {
	auto first = text.find_first_not_of(" \t");
	if (first == std::string::npos)
		return "";
	return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

bool isDigits(const std::string& text) {
	return std::all_of(text.begin(), text.end(),
	                   [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

std::vector<sidebarRow> buildSidebarRows(const catalogue& db, mediaType media) {
	auto count = [&](watchStatus status) {
		return fmt::format(" ({})", db.countByStatus(media, status));
	};
	bool isManga = media == mediaType::kManga;
	std::vector<sidebarRow> rows = {
	    {"  LIBRARY", true, std::nullopt, std::nullopt},
	    {fmt::format("{} All Media    ({})", icon::kLibrary, db.getEntries(media).size()), false,
	     std::nullopt, std::nullopt},
	    {"  STATUS", true, std::nullopt, std::nullopt},
	    {fmt::format("{} {}{}", icon::kWatching, isManga ? "Reading     " : "Watching    ",
	                 count(watchStatus::kWatching)),
	     false, watchStatus::kWatching, std::nullopt},
	    {fmt::format("{} Completed   {}", icon::kCompleted, count(watchStatus::kCompleted)), false,
	     watchStatus::kCompleted, std::nullopt},
	    {fmt::format("{} On-Hold     {}", icon::kOnHold, count(watchStatus::kOnHold)), false,
	     watchStatus::kOnHold, std::nullopt},
	    {fmt::format("{} Dropped     {}", icon::kDropped, count(watchStatus::kDropped)), false,
	     watchStatus::kDropped, std::nullopt},
	    {fmt::format("{} Planned     {}", icon::kPlan, count(watchStatus::kPlanToWatch)), false,
	     watchStatus::kPlanToWatch, std::nullopt},
	    {"  RATINGS", true, std::nullopt, std::nullopt},
	};
	// Every score has its row, 10 down to 0 (unrated), so each can be filtered.
	for (int score = 10; score >= 0; --score) {
		rows.push_back({fmt::format("{:>2} {} ({})", score, renderStarMeter(score),
		                            db.countByScore(media, score)),
		                false, std::nullopt, score});
	}
	return rows;
}

libraryTotals sumUp(const catalogue& db, mediaType media) {
	libraryTotals totals;
	totals.daysSpent = db.daysSpent(media);
	totals.meanScore = db.averageScore(media);
	totals.units = db.totalProgress(media);
	totals.rewatched = db.totalRewatched(media);
	totals.volumes = db.totalVolumes(media);
	for (size_t status = 0; status < totals.byStatus.size(); ++status)
		totals.byStatus[status] = db.countByStatus(media, static_cast<watchStatus>(status));
	totals.titles = db.getEntries(media).size();
	return totals;
}

}  // namespace

// Score, progress and date start high-to-low (see sortEntries), so for them
// the reversed order is the ascending one.
bool catalogTab::isSortAscending(sortKey key, bool reverse) {
	switch (key) {
		case sortKey::kNew:
		case sortKey::kUpdated:
		case sortKey::kScore:
		case sortKey::kProgress:
		case sortKey::kDate:
			return reverse;
		case sortKey::kTitle:
		case sortKey::kStatus:
			return not reverse;
	}
	return false;
}

std::string catalogTab::progressLabel(const entry& e) {
	return e.totalUnits > 0 ? fmt::format("{}/{}", e.progress, e.totalUnits)
	                        : fmt::format("{}/?", e.progress);
}

std::string catalogTab::unitName(mediaType type) {
	return type == mediaType::kAnime ? " eps" : " chs";
}

std::optional<countField> catalogTab::countFieldOf(detailsField field) {
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

// How a count reads in the details card and the status line.
std::string catalogTab::countLabel(const entry& e, countField field) {
	auto knownOrUnknown = [](int count) {
		return count > 0 ? std::to_string(count) : std::string("?");
	};
	switch (field) {
		case countField::kProgress:
			return progressLabel(e);
		case countField::kTotalUnits:
			return knownOrUnknown(e.totalUnits);
		case countField::kVolumes:
			return fmt::format("{}/{}", e.volumes, knownOrUnknown(e.totalVolumes));
		case countField::kRewatched:
			return std::to_string(e.rewatched);
	}
	return "";
}

catalogTab::catalogTab(mediaType media) : media_(media) {}

std::string catalogTab::title() const {
	return media_ == mediaType::kAnime ? "Anime" : "Manga";
}

std::string catalogTab::icon() const {
	return media_ == mediaType::kAnime ? icon::kAnime : icon::kManga;
}

void catalogTab::onActivate(appContext& ctx) {
	refresh(ctx);
}

bool catalogTab::isAdding() const {
	return step_ == step::kAddTyping or step_ == step::kAddSearching or step_ == step::kAddPicking;
}

appMode catalogTab::mode() const {
	switch (step_) {
		case step::kBrowse:
			return appMode::kNor;
		case step::kSearch:
			return appMode::kSch;
		case step::kTypeNumber:
			return appMode::kEdt;
		case step::kAddTyping:
		case step::kAddSearching:
		case step::kAddPicking:
			return appMode::kAdd;
		case step::kConfirmDelete:
			return appMode::kDel;
	}
	return appMode::kNor;
}

std::optional<iTab::overlayImage> catalogTab::image() const {
	return cover_;
}

std::optional<std::string> catalogTab::countBadge(appContext& ctx) const {
	return std::to_string(ctx.db.getEntries(media_).size());
}

std::vector<hint> catalogTab::hints() const {
	switch (step_) {
		case step::kConfirmDelete:
			return {{"y", "delete"}, {"n/Esc", "keep"}};
		case step::kTypeNumber:
			return {{"ret", "save"}, {"Esc", "cancel"}};
		case step::kSearch:
			return {{"ret", "apply"}, {"Esc", "clear"}};
		case step::kAddTyping:
			return {{"ret", "search"}, {"Esc", "cancel"}};
		case step::kAddSearching:
			// The search holds the keyboard until it answers.
			return {};
		case step::kAddPicking:
			return {{"j/k", "move"}, {"ret", "add"}, {"Esc", "cancel"}};
		case step::kBrowse:
			break;
	}
	if (focus_ == focusedCard::kDetails)
		return {{"j/k", "field"}, {"h/l", "adjust"}, {"e/ret", "type"}, {"Esc", "catalog"}};
	return {{"Tab", "tabs"},    {"h/l", "panes"}, {"e/ret", "edit"}, {"n/u/c/t/s/r/p", "sort"},
	        {"S/R/P", "cycle"}, {"+/-", "ep"},    {"a", "add"},      {"d", "del"},
	        {"/", "find"},      {"q", "quit"}};
}

bool catalogTab::passesFilters(const entry& candidate) const {
	if (sidebarSelected_ >= 0 and sidebarSelected_ < static_cast<int>(sidebarRows_.size())) {
		const auto& row = sidebarRows_[sidebarSelected_];
		if (row.status and candidate.status != *row.status)
			return false;
		if (row.score and candidate.score != *row.score)
			return false;
	}
	if (search_.value.empty())
		return true;
	return utf8::lowercase(candidate.title).find(utf8::lowercase(search_.value)) !=
	       std::string::npos;
}

void catalogTab::refresh(appContext& ctx) {
	sidebarRows_ = buildSidebarRows(ctx.db, media_);
	libraryEmpty_ = ctx.db.getEntries(media_).empty();
	totals_ = sumUp(ctx.db, media_);
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
	bool inRange = catalogSelected_ >= 0 and catalogSelected_ < static_cast<int>(displayed_.size());
	return inRange ? &displayed_[catalogSelected_] : nullptr;
}

void catalogTab::selectId(int id) {
	for (size_t row = 0; row < displayed_.size(); ++row) {
		if (displayed_[row].id == id)
			catalogSelected_ = static_cast<int>(row);
	}
}

// Ends whatever step was running and gives the keyboard back to the table.
void catalogTab::browse() {
	step_ = step::kBrowse;
	focus_ = focusedCard::kCatalog;
}

bool catalogTab::onEvent(const Event& event, appContext& ctx) {
	switch (step_) {
		case step::kBrowse:
			return onBrowseKey(event, ctx);
		case step::kSearch:
			return onSearchKey(event, ctx);
		case step::kTypeNumber:
			return onNumberKey(event, ctx);
		case step::kAddTyping:
			return onAddTitleKey(event, ctx);
		case step::kAddSearching:
		case step::kAddPicking:
			return onPickKey(event, ctx);
		case step::kConfirmDelete:
			return onConfirmDeleteKey(event, ctx);
	}
	return false;
}

// The title search runs here, on the next redraw tick after Return, so the
// "Searching" status is on screen while it waits.
void catalogTab::onTick(appContext& ctx) {
	if (step_ != step::kAddSearching)
		return;
	// Jikan is MyAnimeList's own; AniList stands in when Jikan cannot be
	// reached.
	auto found = jikan::search(ctx.fetch, media_, addTitle_.value);
	if (not found.ok())
		found = anilist::search(ctx.post, media_, addTitle_.value);
	addResults_ = found.ok() ? found.value() : std::vector<jikan::title>{};
	addSelected_ = 0;
	step_ = step::kAddPicking;
	std::string quoted = fmt::format("'{}'", utf8::clip(addTitle_.value, 24));
	if (not found.ok())
		ctx.status =
		    fmt::format("Could not reach MyAnimeList; the last row adds {} by hand.", quoted);
	else if (addResults_.empty())
		ctx.status = fmt::format("No matches for {}; the last row adds it by hand.", quoted);
	else
		ctx.status = fmt::format("{} matches for {}.", addResults_.size(), quoted);
}

bool catalogTab::onBrowseKey(const Event& event, appContext& ctx) {
	if (onEntryKey(event, ctx))
		return true;
	if (focus_ != focusedCard::kDetails and onSortKey(event, ctx))
		return true;
	bool handled = focus_ == focusedCard::kCatalog   ? onCatalogKey(event, ctx)
	               : focus_ == focusedCard::kSidebar ? onSidebarKey(event, ctx)
	                                                 : onDetailsKey(event, ctx);
	if (handled)
		return true;

	if (keys::isCharacter(event, '/')) {
		step_ = step::kSearch;
		ctx.status = "Search title... [ret] Apply  [Esc] Clear";
		return true;
	}
	if (keys::isCharacter(event, 'a')) {
		step_ = step::kAddTyping;
		addTitle_.value.clear();
		addResults_.clear();
		ctx.status = "Add: type a title to look up on MyAnimeList.";
		return true;
	}
	if (keys::isCharacter(event, 'd') and selectedEntry()) {
		step_ = step::kConfirmDelete;
		ctx.status =
		    fmt::format("Delete '{}'? Confirm [y/N]", utf8::clip(selectedEntry()->title, 24));
		return true;
	}
	return false;
}

bool catalogTab::onConfirmDeleteKey(const Event& event, appContext& ctx) {
	if (keys::isCharacter(event, 'y') or keys::isCharacter(event, 'Y')) {
		// The selection cannot move while the question is open.
		entry doomed = *selectedEntry();
		browse();
		ctx.db.removeEntry(media_, doomed.id);
		refresh(ctx);
		ctx.save(fmt::format("Deleted '{}'.", utf8::clip(doomed.title, 36)));
		return true;
	}
	bool cancels =
	    keys::isCharacter(event, 'n') or keys::isCharacter(event, 'N') or event == Event::Escape;
	if (cancels) {
		browse();
		ctx.status = "Deletion canceled.";
	}
	return true;
}

bool catalogTab::onSearchKey(const Event& event, appContext& ctx) {
	if (event == Event::Escape) {
		search_.value.clear();
		refresh(ctx);
		browse();
		ctx.status = "Search cleared.";
		return true;
	}
	if (keys::isConfirm(event) or event == Event::ArrowDown) {
		refresh(ctx);
		browse();
		ctx.status = search_.value.empty() ? "Filter cleared."
		                                   : fmt::format("Filtered: '{}' ({} matches)",
		                                                 search_.value, displayed_.size());
		return true;
	}
	if (search_.handle(event))
		refresh(ctx);
	return true;
}

bool catalogTab::onNumberKey(const Event& event, appContext& ctx) {
	if (keys::isConfirm(event)) {
		commitNumber(ctx);
		return true;
	}
	if (event == Event::Escape) {
		step_ = step::kBrowse;
		ctx.status = fmt::format("{} left unchanged.", fieldLabel(detailsCursor_));
		return true;
	}
	// Digits only (a paste arrives as one event), and few enough that the
	// number always fits in an int.
	bool fits = not event.is_character() or
	            (isDigits(event.character()) and
	             number_.value.size() + event.character().size() <= kMaxNumberDigits);
	if (fits)
		number_.handle(event);
	return true;
}

bool catalogTab::onAddTitleKey(const Event& event, appContext& ctx) {
	if (event == Event::Escape) {
		browse();
		ctx.status = "Add canceled.";
		return true;
	}
	if (not keys::isConfirm(event)) {
		addTitle_.handle(event);
		return true;
	}
	addTitle_.value = trimmed(addTitle_.value);
	if (addTitle_.value.empty()) {
		ctx.status = "Type a title first, or [Esc] to cancel.";
		return true;
	}
	step_ = step::kAddSearching;
	ctx.status = fmt::format("Searching MyAnimeList for '{}'...", utf8::clip(addTitle_.value, 24));
	return true;
}

bool catalogTab::onPickKey(const Event& event, appContext& ctx) {
	if (event == Event::Escape) {
		browse();
		ctx.status = "Add canceled.";
		return true;
	}
	if (step_ == step::kAddSearching)
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

bool catalogTab::onEntryKey(const Event& event, appContext& ctx) {
	const entry* current = selectedEntry();
	bool increments = keys::isCharacter(event, 'P') or keys::isIncrement(event);
	bool acts = keys::isCharacter(event, 'S') or keys::isCharacter(event, 'R') or increments or
	            keys::isCharacter(event, '-');
	if (not acts)
		return false;
	if (not current)
		return true;

	isoDate today = ctx.today();
	if (keys::isCharacter(event, 'S')) {
		entry changed = cycleStatus(*current, 1, today);
		commit(ctx, changed, fmt::format("Status: {}", statusToString(changed.status, media_)));
	} else if (keys::isCharacter(event, 'R')) {
		entry changed = adjustRating(*current, 1, today);
		commit(ctx, changed, fmt::format("Rating: {}", renderStarMeter(changed.score)));
	} else {
		entry changed = stepProgress(*current, increments ? 1 : -1, today);
		commit(ctx, changed, fmt::format("Progress: {}", progressLabel(changed)));
	}
	return true;
}

bool catalogTab::onSortKey(const Event& event, appContext& ctx) {
	const std::pair<char, std::pair<sortKey, const char*>> sortKeys[] = {
	    {'n', {sortKey::kNew, "New releases"}}, {'u', {sortKey::kUpdated, "Last updated"}},
	    {'c', {sortKey::kDate, "Date"}},        {'t', {sortKey::kTitle, "Title"}},
	    {'s', {sortKey::kStatus, "Status"}},    {'r', {sortKey::kScore, "Rating"}},
	    {'p', {sortKey::kProgress, "Progress"}}};
	for (const auto& [key, column] : sortKeys) {
		if (not keys::isCharacter(event, key))
			continue;
		toggleSort(column.first, column.second, ctx);
		return true;
	}
	return false;
}

bool catalogTab::onCatalogKey(const Event& event, appContext& ctx) {
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
	if (keys::isConfirm(event) or keys::isCharacter(event, 'e')) {
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

bool catalogTab::onSidebarKey(const Event& event, appContext& ctx) {
	if (keys::isMoveRight(event) or keys::isConfirm(event)) {
		focus_ = focusedCard::kCatalog;
		return true;
	}
	if (keys::isMoveDown(event) or keys::isMoveUp(event)) {
		moveSidebarCursor(keys::isMoveDown(event) ? 1 : -1, ctx);
		return true;
	}
	return false;
}

bool catalogTab::onDetailsKey(const Event& event, appContext& ctx) {
	if (event == Event::Escape) {
		browse();
		return true;
	}
	if (keys::isMoveDown(event) or keys::isMoveUp(event)) {
		moveDetailsCursor(keys::isMoveDown(event) ? 1 : -1);
		return true;
	}
	const entry* current = selectedEntry();
	if (not current)
		return false;
	if (keys::isMoveLeft(event) or keys::isMoveRight(event)) {
		adjustDetailsField(ctx, keys::isMoveRight(event) ? 1 : -1);
		return true;
	}
	bool activates = keys::isConfirmOrSpace(event) or keys::isCharacter(event, 'e');
	if (not activates)
		return false;
	auto field = countFieldOf(detailsCursor_);
	if (not field) {
		adjustDetailsField(ctx, 1);
		return true;
	}
	number_.value = std::to_string(countOf(*current, *field));
	step_ = step::kTypeNumber;
	ctx.status = fmt::format("{} type a number.", fieldLabel(detailsCursor_));
	return true;
}

void catalogTab::commit(appContext& ctx, const entry& changed, const std::string& done) {
	ctx.db.updateEntry(changed);
	refresh(ctx);
	selectId(changed.id);
	ctx.save(done);
	// Touching a title is the moment to see whether MyAnimeList knows more
	// about it now (a final episode count, new chapters).
	ctx.web.lookUp(media_, changed.malId);
}

void catalogTab::adjustDetailsField(appContext& ctx, int direction) {
	const entry* current = selectedEntry();
	if (not current)
		return;
	isoDate today = ctx.today();
	if (detailsCursor_ == detailsField::kStatus) {
		entry changed = cycleStatus(*current, direction, today);
		commit(ctx, changed, fmt::format("Status: {}", statusToString(changed.status, media_)));
	} else if (detailsCursor_ == detailsField::kRating) {
		entry changed = adjustRating(*current, direction, today);
		commit(ctx, changed, fmt::format("Rating: {}", renderStarMeter(changed.score)));
	} else {
		countField field = *countFieldOf(detailsCursor_);
		entry changed = setCount(*current, field, countOf(*current, field) + direction, today);
		commit(ctx, changed,
		       fmt::format("{} {}", fieldLabel(detailsCursor_), countLabel(changed, field)));
	}
}

void catalogTab::commitNumber(appContext& ctx) {
	if (number_.value.empty()) {
		ctx.status = "Type a number, or [Esc] to cancel.";
		return;
	}
	step_ = step::kBrowse;
	const entry* current = selectedEntry();
	auto field = countFieldOf(detailsCursor_);
	if (not current or not field)
		return;
	entry changed = setCount(*current, *field, std::stoi(number_.value), ctx.today());
	commit(ctx, changed,
	       fmt::format("{} {}", fieldLabel(detailsCursor_), countLabel(changed, *field)));
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
		return {detailsField::kStatus, detailsField::kRating, detailsField::kProgress,
		        detailsField::kTotal, detailsField::kRewatched};
	return {detailsField::kStatus, detailsField::kRating,  detailsField::kProgress,
	        detailsField::kTotal,  detailsField::kVolumes, detailsField::kRewatched};
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
void catalogTab::toggleSort(sortKey key, const std::string& name, appContext& ctx) {
	reverseSort_ = sortBy_ == key and not reverseSort_;
	sortBy_ = key;
	refresh(ctx);
	ctx.status =
	    fmt::format("Sort: {} ({})", name,
	                isSortAscending(sortBy_, reverseSort_) ? "Increasing ▲" : "Decreasing ▼");
}

void catalogTab::moveSidebarCursor(int direction, appContext& ctx) {
	int candidate = sidebarSelected_ + direction;
	int lastRow = static_cast<int>(sidebarRows_.size()) - 1;
	while (candidate >= 0 and candidate <= lastRow and sidebarRows_[candidate].isHeader)
		candidate += direction;
	if (candidate < 1 or candidate > lastRow)
		return;
	sidebarSelected_ = candidate;
	refresh(ctx);
}

void catalogTab::addPicked(appContext& ctx) {
	entry added;
	added.type = media_;
	added.status = watchStatus::kPlanToWatch;
	added.updatedAt = ctx.today();
	added.title = addTitle_.value;
	std::optional<entry> existing;
	if (addSelected_ < static_cast<int>(addResults_.size())) {
		const auto& pick = addResults_[addSelected_];
		added.title = pick.name;
		added.malId = pick.malId;
		added.totalUnits = pick.totalUnits;
		added.totalVolumes = pick.totalVolumes;
		existing = ctx.db.findByMalId(media_, pick.malId);
	}

	browse();
	// Show the whole list so the title is there to be seen.
	search_.value.clear();
	sidebarSelected_ = 1;
	std::string name = fmt::format("'{}'", utf8::clip(added.title, 36));
	if (existing) {
		refresh(ctx);
		selectId(existing->id);
		ctx.status = fmt::format("{} is already in the catalogue.", name);
		return;
	}
	int id = ctx.db.addEntry(added);
	refresh(ctx);
	selectId(id);
	ctx.save(fmt::format("Added {}.", name));
	ctx.web.lookUp(media_, added.malId);
}

}  // namespace catalist
