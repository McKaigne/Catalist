// Drawing for catalogTab: the sidebar, the search bar, the table (or the
// title search's matches) and the details card. Keys and state are in
// catalogTab.cpp.

#include <fmt/format.h>

#include <algorithm>
#include <cmath>

#include "text.hpp"
#include "tui/catalogTab.hpp"

namespace catalist {

using namespace ftxui;

namespace {

// Titles get whatever the fixed columns leave; off a terminal that is 16.
// For a table `width` cells wide inside its border; before the first frame
// has measured it (width 0), everything is shown. On a narrow terminal the
// date goes first, then the new count, then the rating.
tableLayout fitTable(int width) {
	constexpr int kAlways = 2 + 4 + 13;  // marker, type icon, progress
	constexpr int kDate = 11, kFresh = 7, kRating = 13, kStatusName = 15, kMinTitle = 16;
	tableLayout layout;
	if (width <= 0)
		return layout;
	int used = kAlways + 2 + kDate + kFresh + kRating + kStatusName;
	if (used + kMinTitle > width) {
		layout.date = false;
		used -= kDate;
	}
	if (used + kMinTitle > width) {
		layout.fresh = false;
		used -= kFresh;
	}
	if (used + kMinTitle > width) {
		layout.rating = false;
		used -= kRating;
	}
	// Last of all, the status keeps only its icon.
	if (used + kMinTitle > width) {
		layout.statusName = false;
		used -= kStatusName;
	}
	layout.titleGlyphs = static_cast<size_t>(std::max(8, width - used));
	return layout;
}

// Inside width of a card drawn last frame at `box`, or 0 before that.
int insideWidth(const Box& box) {
	int width = box.x_max - box.x_min + 1 - 2;
	return width > 0 ? width : 0;
}

// Wait this long after the selection last moved before asking for a cover,
// so scrolling through the list fetches nothing.
constexpr uint64_t kCoverDelayMs = 400;

// Both side panels are this wide, the cover's card included.
constexpr int kSidePanelWidth = 32;
// Rows the cover keeps before the details card gives way, and that card's
// height.
constexpr int kMinCoverRows = 10;
constexpr int kDetailsRows = 9;
// Covers are about 7 wide to 10 tall.
constexpr double kCoverAspect = 0.7;

std::string formatDateCell(const std::optional<isoDate>& date) {
	return date ? date->toString() : "    -     ";
}

Element styledText(const std::string& content, Color tint, bool emphasised) {
	auto element = text(content) | color(tint);
	return emphasised ? element | bold : element;
}

// The title with every case-insensitive match of the search marked.
Element highlightMatches(const std::string& title, const std::string& query, bool emphasised) {
	std::string loweredTitle = utf8::lowercase(title);
	std::string loweredQuery = utf8::lowercase(query);
	size_t match = query.empty() ? std::string::npos : loweredTitle.find(loweredQuery);
	if (match == std::string::npos)
		return styledText(title, theme::kText, emphasised);

	Elements parts;
	size_t consumed = 0;
	while (match != std::string::npos) {
		if (match > consumed)
			parts.push_back(
			    styledText(title.substr(consumed, match - consumed), theme::kText, emphasised));
		parts.push_back(text(title.substr(match, loweredQuery.size())) | bold |
		                color(theme::kCrust) | bgcolor(theme::kYellow));
		consumed = match + loweredQuery.size();
		match = loweredTitle.find(loweredQuery, consumed);
	}
	if (consumed < title.size())
		parts.push_back(styledText(title.substr(consumed), theme::kText, emphasised));
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
	return fmt::format("{}.{}", withCommas(tenths / 10), tenths % 10);
}

std::string twoDecimals(double value) {
	return fmt::format("{:.2f}", value);
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
		int cellsWide = last ? width - used : static_cast<int>(count * width / totals.titles);
		if (count > 0 and cellsWide == 0)
			cellsWide = 1;
		cellsWide = std::min(cellsWide, width - used);
		used += cellsWide;
		if (cellsWide > 0)
			cells.push_back(text(std::string(cellsWide, ' ')) |
			                bgcolor(statusToColor(static_cast<watchStatus>(status))));
	}
	return hbox(std::move(cells));
}

// What the cover card says while there is no picture to show.
std::string coverNote(updater::coverState state) {
	switch (state) {
		case updater::coverState::kLoading:
		case updater::coverState::kReady:
			return "Loading cover...";
		case updater::coverState::kUnreachable:
			return "Could not reach MyAnimeList for the cover; trying again shortly.";
		case updater::coverState::kNone:
			return "MyAnimeList has no cover for this title.";
		case updater::coverState::kUnreadable:
			return "img2sixel could not read the cover image.";
	}
	return "";
}

Element numberBox(const textField& field) {
	return field.render("0", true) | bgcolor(theme::kSurface0) | size(WIDTH, EQUAL, 8);
}

}  // namespace

Element catalogTab::render(appContext& ctx) {
	bool picking = step_ == step::kAddSearching or step_ == step::kAddPicking;
	Element middle = picking ? renderPickCard() : renderCatalogCard();
	return hbox({vbox({renderSidebarCard() | flex, renderStatsCard()}) |
	                 size(WIDTH, EQUAL, kSidePanelWidth),
	             vbox({renderSearchCard(), middle | flex}) | flex, renderSideColumn(ctx)});
}

Element catalogTab::renderSidebarRow(int row) const {
	const auto& item = sidebarRows_[row];
	if (item.isHeader)
		return text(item.label) | bold | color(theme::kOverlay0);

	bool isFilter = row == sidebarSelected_;
	bool cursorHere = isFilter and step_ != step::kSearch and focus_ == focusedCard::kSidebar;
	std::string prefix = isFilter ? fmt::format("{} ", icon::kCheckSingle) : "   ";
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
	bool focused = step_ != step::kSearch and focus_ == focusedCard::kSidebar;
	Elements rows;
	for (size_t row = 0; row < sidebarRows_.size(); ++row)
		rows.push_back(renderSidebarRow(static_cast<int>(row)));
	return vbox({text("  CATEGORIES") | bold | color(theme::kLavender), text(""),
	             vbox(std::move(rows)) | vscroll_indicator | frame | flex}) |
	       border | color(focused ? theme::kMauve : theme::kSurface0);
}

// Below the categories, which shrink and scroll first on a short terminal.
Element catalogTab::renderStatsCard() const {
	bool anime = media_ == mediaType::kAnime;
	auto row = [](const std::string& label, const std::string& value) {
		return hbox({text(fmt::format("  {}", label)) | color(theme::kSubtext), filler(),
		             text(value) | bold | color(theme::kText), text("  ")});
	};
	Elements lines = {text("  STATS") | bold | color(theme::kLavender),
	                  text(""),
	                  row("Days spent", oneDecimal(totals_.daysSpent)),
	                  row("Mean score", twoDecimals(totals_.meanScore)),
	                  row(anime ? "Episodes" : "Chapters", withCommas(totals_.units)),
	                  row(anime ? "Rewatched" : "Reread", withCommas(totals_.rewatched))};
	if (not anime)
		lines.push_back(row("Volumes", withCommas(totals_.volumes)));
	lines.push_back(hbox({text("  "), statusBar(totals_, kSidePanelWidth - 4)}));
	return vbox(std::move(lines)) | border | color(theme::kSurface0);
}

Element catalogTab::renderSearchCard() const {
	if (isAdding())
		return hbox({text(" + ") | bold | color(theme::kBlue),
		             addTitle_.render("Title to add...", step_ == step::kAddTyping) | flex,
		             text(fmt::format("Add to {} ", utf8::lowercase(title()))) |
		                 color(theme::kSubtext)}) |
		       border | color(theme::kBlue) | size(HEIGHT, EQUAL, 3);
	bool searching = step_ == step::kSearch;
	// The summary only shows when the bar has room for it beside the field.
	std::string summary;
	if (insideWidth(tableBox_) == 0 or insideWidth(tableBox_) >= 50)
		summary =
		    search_.value.empty() ? "All entries " : fmt::format("Filter: '{}' ", search_.value);
	return hbox({text(fmt::format(" {} ", icon::kSearch)) | color(theme::kPeach),
	             search_.render("Press '/' to search...", searching) | flex,
	             text(summary) | color(theme::kSubtext), text(" ")}) |
	       border | color(searching ? theme::kPeach : theme::kSurface0) | size(HEIGHT, EQUAL, 3);
}

Element catalogTab::renderHeaderCell(const std::string& name, sortKey key, int width,
                                     bool stretches) const {
	bool active = sortBy_ == key and not isAdding();
	std::string label = fmt::format(" {}", name);
	if (active)
		label += isSortAscending(key, reverseSort_) ? " ▲" : " ▼";
	Element cell = text(label);
	cell = stretches ? cell | flex : cell | size(WIDTH, EQUAL, width);
	return active ? cell | bold | color(theme::kMauve) | underlined
	              : cell | bold | color(theme::kSubtext);
}

Element catalogTab::renderCatalogRow(int row, const tableLayout& layout) {
	const auto& e = displayed_[row];
	bool rowActive =
	    step_ != step::kSearch and row == catalogSelected_ and focus_ != focusedCard::kSidebar;
	std::string shownTitle = rowActive ? marquee::scroll(e.title, layout.titleGlyphs,
	                                                     rowTimer_.elapsedFor(catalogSelected_))
	                                   : utf8::clip(e.title, layout.titleGlyphs);

	Elements cells = {text(rowActive ? "▌ " : "  ") | bold | color(theme::kMauve)};
	if (layout.date)
		cells.push_back(
		    hbox({text(formatDateCell(sortBy_ == sortKey::kUpdated ? e.updatedAt : listDate(e))) |
		              size(WIDTH, EQUAL, 10) | color(theme::kSubtext),
		          text(" ")}));
	cells.push_back(hbox({text(fmt::format(" {} ", typeToIcon(e.type))),
	                      highlightMatches(shownTitle, search_.value, rowActive)}) |
	                flex);
	cells.push_back(text(" "));
	if (layout.statusName)
		cells.push_back(
		    hbox({text(fmt::format("{} ", statusToIcon(e.status))) | color(statusToColor(e.status)),
		          text(statusToString(e.status, e.type)) | color(statusToColor(e.status))}) |
		    size(WIDTH, EQUAL, 16));
	else
		cells.push_back(text(statusToIcon(e.status)) | color(statusToColor(e.status)) |
		                size(WIDTH, EQUAL, 2));
	if (layout.rating)
		cells.push_back(hbox({text(" "), text(fmt::format(" {}", renderStarMeter(e.score))) |
		                                     size(WIDTH, EQUAL, 12) | color(theme::kYellow)}));
	cells.push_back(hbox({text(" "), text(fmt::format(" {} {}", icon::kGauge, progressLabel(e))) |
	                                     size(WIDTH, EQUAL, 12) | color(theme::kLavender)}));
	if (layout.fresh)
		cells.push_back(
		    hbox({text(" "), text(newReleases(e) > 0 ? fmt::format(" +{}", newReleases(e)) : "") |
		                         bold | size(WIDTH, EQUAL, 6) | color(theme::kGreen)}));
	cells.push_back(text(" "));
	auto line = hbox(std::move(cells));
	return rowActive ? line | bgcolor(theme::kSurface0) | focus : line;
}

Element catalogTab::renderCatalogCard() {
	tableLayout layout = fitTable(insideWidth(tableBox_));
	Elements heads = {text("  ")};
	if (layout.date)
		heads.push_back(hbox({sortBy_ == sortKey::kUpdated
		                          ? renderHeaderCell("Updated", sortKey::kUpdated, 10, false)
		                          : renderHeaderCell("Date", sortKey::kDate, 10, false),
		                      text(" ")}));
	heads.push_back(hbox({text("   "), renderHeaderCell("Title", sortKey::kTitle, 28, true)}) |
	                flex);
	heads.push_back(text(" "));
	heads.push_back(layout.statusName ? renderHeaderCell("Status", sortKey::kStatus, 16, false)
	                                  : text("  "));
	if (layout.rating)
		heads.push_back(hbox({text(" "), renderHeaderCell("Rating", sortKey::kScore, 12, false)}));
	heads.push_back(hbox({text(" "), renderHeaderCell("Progress", sortKey::kProgress, 12, false)}));
	if (layout.fresh)
		heads.push_back(hbox({text(" "), renderHeaderCell("New", sortKey::kNew, 6, false)}));
	heads.push_back(text(" "));

	Element rows;
	if (displayed_.empty()) {
		rows = paragraphAlignCenter(libraryEmpty_
		                                ? "Nothing here yet. Press a to add a title, or run: "
		                                  "catalist import <file.xml>"
		                                : "No matching entries found.") |
		       color(theme::kSubtext) | vcenter | flex;
	} else {
		Elements lines;
		for (size_t row = 0; row < displayed_.size(); ++row)
			lines.push_back(renderCatalogRow(static_cast<int>(row), layout));
		rows = vbox(std::move(lines)) | vscroll_indicator | frame | flex;
	}
	bool focused = step_ != step::kSearch and focus_ == focusedCard::kCatalog;
	return vbox({hbox(std::move(heads)), text(""), rows}) | border |
	       color(focused ? theme::kMauve : theme::kSurface0) | flex | reflect(tableBox_);
}

Element catalogTab::renderPickCard() {
	auto heading = [](const std::string& name, int width) {
		Element cell = text(fmt::format(" {}", name)) | bold | color(theme::kSubtext);
		return width > 0 ? cell | size(WIDTH, EQUAL, width) : cell | flex;
	};
	auto header = hbox({text("   ") | size(WIDTH, EQUAL, 3), heading("Year", 10), text(" "),
	                    heading("Title", 0), text(" "), heading("Length", 14), text(" "),
	                    heading("Score", 12), text(" ")});
	if (step_ == step::kAddSearching)
		return vbox({header, text(""),
		             text(" Searching MyAnimeList...") | color(theme::kSubtext) | center | flex}) |
		       border | color(theme::kBlue) | flex | reflect(tableBox_);

	// Titles get what the year, length and score columns leave.
	int width = insideWidth(tableBox_);
	size_t pickTitleGlyphs = width > 0 ? static_cast<size_t>(std::max(8, width - 45)) : 16;

	Elements lines;
	for (size_t row = 0; row <= addResults_.size(); ++row) {
		bool active = static_cast<int>(row) == addSelected_;
		Element line;
		if (row == addResults_.size()) {
			line = hbox({text(active ? "▌ " : "  ") | bold | color(theme::kMauve),
			             text("    +     ") | color(theme::kBlue), text(" "),
			             text(fmt::format(" Add '{}' by hand",
			                              utf8::clip(addTitle_.value, pickTitleGlyphs))) |
			                 color(theme::kSubtext) | flex});
		} else {
			const auto& match = addResults_[row];
			std::string score = match.score > 0 ? fmt::format("{:.2f}", match.score) : "-";
			std::string length = match.totalUnits > 0
			                         ? fmt::format("{}{}", match.totalUnits, unitName(media_))
			                         : fmt::format("?{}", unitName(media_));
			std::string name = active ? marquee::scroll(match.name, pickTitleGlyphs,
			                                            rowTimer_.elapsedFor(addSelected_))
			                          : utf8::clip(match.name, pickTitleGlyphs);
			line = hbox({text(active ? "▌ " : "  ") | bold | color(theme::kMauve),
			             text(match.year.empty() ? "    -" : fmt::format("   {}", match.year)) |
			                 size(WIDTH, EQUAL, 10) | color(theme::kSubtext),
			             text(" "),
			             text(fmt::format(" {}", name)) | (active ? bold : nothing) |
			                 color(theme::kText) | flex,
			             text(" "),
			             text(fmt::format(" {} {}", icon::kGauge, length)) |
			                 size(WIDTH, EQUAL, 14) | color(theme::kLavender),
			             text(" "),
			             text(fmt::format(" {} {}", icon::kStarFull, score)) |
			                 size(WIDTH, EQUAL, 12) | color(theme::kYellow),
			             text(" ")});
		}
		lines.push_back(active ? line | bgcolor(theme::kSurface0) | focus : line);
	}
	return vbox({header, text(""), vbox(std::move(lines)) | vscroll_indicator | frame | flex}) |
	       border | color(theme::kBlue) | flex | reflect(tableBox_);
}

Element catalogTab::renderDetails(bool withDetails) {
	const entry* current = selectedEntry();
	if (not current)
		return vbox({text("  DETAILS") | bold | color(theme::kLavender),
		             text("  No title selected.") | color(theme::kSubtext)}) |
		       border | color(theme::kSurface0);
	const auto& e = *current;

	// Shared pieces: how far along the title is, and the row layouts.
	float ratio = e.totalUnits > 0
	                  ? std::clamp(static_cast<float>(e.progress) / e.totalUnits, 0.0f, 1.0f)
	                  : 0.0f;
	bool finished = ratio >= 1.0f or e.status == watchStatus::kCompleted;
	bool onDetails = focus_ == focusedCard::kDetails;
	bool typing = step_ == step::kTypeNumber;
	auto fieldRow = [&](detailsField field, const std::string& label, Element content) {
		bool active = onDetails and detailsCursor_ == field;
		return hbox({text(active ? "▌ " : "  ") | color(theme::kMauve) | bold,
		             text(label) | size(WIDTH, EQUAL, 11) | (active ? bold : nothing) |
		                 color(active ? theme::kPeach : theme::kSubtext),
		             content | flex});
	};
	auto infoRow = [&](const std::string& label, const std::string& value) {
		return hbox(
		    {text(label) | color(theme::kSubtext), text(value) | color(theme::kText), filler()});
	};

	// The details card: what the title is and when it was watched.
	std::string shownTitle =
	    marquee::scroll(e.title, kSidePanelWidth - 6, detailsTimer_.elapsedFor(catalogSelected_));
	std::string unfinished = e.status == watchStatus::kCompleted ? "Completed" : "In progress";
	auto seriesCard =
	    vbox({text("  DETAILS") | bold | color(theme::kLavender),
	          hbox({text(fmt::format("  {}", shownTitle)) | bold | color(theme::kMauve)}),
	          hbox({text("  Type:     ") | color(theme::kSubtext),
	                pill(fmt::format("{} {}", typeToIcon(e.type), typeToString(e.type)),
	                     theme::kCrust, typeToColor(e.type)),
	                filler()}),
	          infoRow("  Link:     ", e.malId > 0 ? fmt::format("MAL #{}", e.malId) : "-"),
	          infoRow("  Updated:  ", e.updatedAt ? e.updatedAt->toString() : "-"),
	          infoRow("  Started:  ", e.startDate ? e.startDate->toString() : "Not started"),
	          infoRow("  Finished: ", e.finishDate ? e.finishDate->toString() : unfinished)}) |
	    border | color(theme::kSurface0);

	// The tracking card: the fields the cursor can change, with the number
	// being typed drawn in place of its value.
	auto countRow = [&](detailsField field) {
		bool editing = typing and detailsCursor_ == field;
		Element value = editing ? numberBox(number_)
		                        : text(countLabel(e, *countFieldOf(field))) | color(theme::kText);
		return fieldRow(field, fieldLabel(field), hbox({value, filler()}));
	};
	Element progressLine =
	    typing and detailsCursor_ == detailsField::kProgress
	        ? hbox({numberBox(number_),
	                text(fmt::format(" / {}{}", countLabel(e, countField::kTotalUnits),
	                                 unitName(e.type))) |
	                    color(theme::kText)})
	        : text(fmt::format("{}{}", progressLabel(e), unitName(e.type))) | color(theme::kText);
	std::string percent =
	    e.totalUnits > 0 ? fmt::format("{}% ", static_cast<int>(ratio * 100)) : "Ongoing ";

	bool trackingActive = step_ != step::kSearch and onDetails;
	Elements tracking = {
	    hbox({text("  TRACKING") | bold | color(theme::kLavender), filler(),
	          text(trackingActive ? (typing ? "[Editing]" : "[Active]") : "") |
	              color(theme::kPeach)}),
	    fieldRow(
	        detailsField::kStatus, "Status:",
	        pill(fmt::format("{} {}", statusToIcon(e.status), statusToString(e.status, e.type)),
	             theme::kCrust, statusToColor(e.status))),
	    fieldRow(detailsField::kRating, "Rating:",
	             hbox({text(renderStarMeter(e.score)) | color(theme::kYellow),
	                   text(fmt::format(" ({}/10)", e.score)) | color(theme::kText)})),
	    fieldRow(detailsField::kProgress, "Progress:",
	             vbox({hbox({progressLine, filler(), text(percent) | color(theme::kSubtext)}),
	                   gauge(ratio) | color(finished ? theme::kGreen : theme::kBlue)})),
	    countRow(detailsField::kTotal)};
	if (media_ == mediaType::kManga)
		tracking.push_back(countRow(detailsField::kVolumes));
	tracking.push_back(countRow(detailsField::kRewatched));
	Color trackingBorder = typing           ? theme::kPeach
	                       : trackingActive ? theme::kMauve
	                                        : theme::kSurface0;
	// Tracking first, under the cover; the details card is left out when the
	// terminal is too short for it.
	Element trackingCard = vbox(std::move(tracking)) | border | color(trackingBorder);
	return withDetails ? vbox({trackingCard, seriesCard}) : trackingCard;
}

// The cover sits above the tracking and details cards. FTXUI cannot draw pictures, so this card
// is drawn with only a note in it, and the shell paints the sixel over its
// inside after every frame (see image()).
Element catalogTab::renderCoverCard(appContext& ctx) {
	cover_.reset();
	const entry* current = selectedEntry();
	std::string note;
	int columns = coverBox_.x_max - coverBox_.x_min + 1;
	int rows = coverBox_.y_max - coverBox_.y_min + 1;
	bool resting = detailsTimer_.elapsedFor(catalogSelected_) >= kCoverDelayMs;
	if (not current) {
		note = "";
	} else if (current->malId <= 0) {
		note = "No cover: not on MyAnimeList.";
	} else if (not ctx.coverProblem.empty()) {
		note = ctx.coverProblem;
	} else if (rows < 3 or columns < 3 or not resting) {
		note = "Loading cover...";
	} else {
		// Fit a 7 by 10 picture inside the card.
		int heightPx = rows * ctx.cellHeight;
		int widthPx = static_cast<int>(heightPx * kCoverAspect);
		if (widthPx > columns * ctx.cellWidth) {
			widthPx = columns * ctx.cellWidth;
			heightPx = static_cast<int>(widthPx / kCoverAspect);
		}
		// Sixel draws six pixel rows at a time; round down so the picture
		// never reaches the card's border.
		heightPx -= heightPx % 6;
		auto picture = ctx.web.coverFor(media_, current->malId, widthPx, heightPx);
		if (picture.state == updater::coverState::kReady) {
			// Centred both ways in the card.
			int pictureColumns = (widthPx + ctx.cellWidth - 1) / ctx.cellWidth;
			int pictureRows = (heightPx + ctx.cellHeight - 1) / ctx.cellHeight;
			cover_ = overlayImage{coverBox_.x_min + (columns - pictureColumns) / 2,
			                      coverBox_.y_min + (rows - pictureRows) / 2, picture.sixel};
		} else {
			note = coverNote(picture.state);
		}
	}
	Element inside =
	    paragraphAlignCenter(note) | color(theme::kOverlay0) | vcenter | flex | reflect(coverBox_);
	return vbox({inside}) | border | color(theme::kSurface0);
}

// The right-hand column: the cover takes whatever height the tracking and
// details cards leave. On a short terminal the details card goes first, so
// the cover keeps at least kMinCoverRows.
Element catalogTab::renderSideColumn(appContext& ctx) {
	int height = sideBox_.y_max - sideBox_.y_min + 1;
	int trackingRows = media_ == mediaType::kManga ? 10 : 9;
	bool measured = height > 1;
	bool withDetails = not measured or height >= kMinCoverRows + trackingRows + kDetailsRows;
	return vbox({renderCoverCard(ctx) | flex, renderDetails(withDetails)}) |
	       size(WIDTH, EQUAL, kSidePanelWidth) | reflect(sideBox_);
}

}  // namespace catalist
