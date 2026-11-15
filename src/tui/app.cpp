#include "tui/app.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <limits>
#include <thread>

#include "system.hpp"
#include "tui/catalogTab.hpp"
#include "tui/style.hpp"
#include "version.hpp"

namespace catalist {

using namespace ftxui;

namespace {

// Redraw cadence for the scrolling titles in the catalogue; also how soon a
// title search starts after Return.
constexpr auto kTickInterval = std::chrono::milliseconds(250);

bool isBackTab(const Event& event) {
	return event == Event::TabReverse or event == Event::Special("\033[Z");
}

Element tabLabel(int number, bool active, const std::string& label, const std::string& glyph,
                 const std::string& count) {
	std::string shown = fmt::format(" {} {} {} ", number, glyph, label);
	if (not count.empty())
		shown += fmt::format("({}) ", count);
	return text(shown) | (active ? bold | color(theme::kMauve) : color(theme::kSubtext));
}

// Counted in glyphs, not bytes, so "▼" is one.
std::string truncated(const std::string& message, int maxLength) {
	if (static_cast<int>(marquee::splitGlyphs(message).size()) <= maxLength)
		return message;
	return fmt::format("{}...", marquee::clip(message, std::max(0, maxLength - 3)));
}

Element modePill(appMode mode) {
	switch (mode) {
		case appMode::kNor:
			return pill(" NOR ", theme::kCrust, theme::kMauve);
		case appMode::kSch:
			return pill(" SCH ", theme::kCrust, theme::kPeach);
		case appMode::kEdt:
			return pill(" EDT ", theme::kCrust, theme::kGreen);
		case appMode::kAdd:
			return pill(" ADD ", theme::kCrust, theme::kBlue);
		case appMode::kDel:
			return pill(" DEL ", theme::kCrust, theme::kRed);
	}
	return text("");
}

// The status line keeps at least this much of the bottom bar.
constexpr int kMinStatusWidth = 30;

// Jikan allows sixty requests a minute and AniList thirty when it is busy;
// one every two seconds stays clear of both, even while the daily check
// runs.
constexpr auto kRequestPause = std::chrono::milliseconds(2000);

std::string coverProblem(const App& screen, bool canConvert) {
	if (not canConvert)
		return "Covers need img2sixel (libsixel).";
	// The terminal lists 4 among its device attributes when it draws sixel.
	const auto& abilities = screen.TerminalCapabilities();
	if (std::find(abilities.begin(), abilities.end(), 4) == abilities.end())
		return "This terminal cannot show sixel images.";
	return "";
}

// Draws a sixel picture at a cell, then puts the cursor back where FTXUI
// left it, so its next frame lands in the right place.
void drawImage(const iTab::overlayImage& picture) {
	std::cout << "\0337\033[" << picture.row + 1 << ";" << picture.column + 1 << "H"
	          << picture.sixel << "\0338" << std::flush;
}

}  // namespace

// Folds finished web lookups into the catalogue. A change the person can see
// (a new total, new episodes or chapters) is named in the status line.
void appShell::applyFindings() {
	auto findings = ctx_.web.takeFindings();
	if (findings.empty())
		return;
	isoDate today = ctx_.today();
	std::vector<std::string> changedTitles;
	for (const auto& found : findings) {
		auto before = ctx_.db.findByMalId(found.type, found.malId);
		if (not before)
			continue;
		entry after = applyWebFacts(*before, found.facts, today);
		ctx_.db.updateEntry(after);
		if (after.totalUnits != before->totalUnits or newReleases(after) != newReleases(*before))
			changedTitles.push_back(after.title);
	}
	if (changedTitles.size() == 1)
		ctx_.save(fmt::format("Updated from MyAnimeList: '{}'.",
		                      marquee::clip(changedTitles.front(), 36)));
	else if (changedTitles.size() > 1)
		ctx_.save(fmt::format("Updated from MyAnimeList: {} titles.", changedTitles.size()));
	else
		ctx_.save(ctx_.status);
	activeTab().onActivate(ctx_);
}

void appContext::save(const std::string& done) {
	status = db.saveAll()
	             ? done
	             : fmt::format("Could not save the catalogue to {}.", db.getConfigPath().string());
}

appShell::appShell(appContext& ctx, std::vector<iTab*> tabs, std::function<void()> quit)
    : ctx_(ctx), tabs_(std::move(tabs)), quit_(std::move(quit)) {
	activeTab().onActivate(ctx_);
}

void appShell::activate(int index) {
	if (index < 0 or index >= static_cast<int>(tabs_.size()))
		return;
	activeIndex_ = index;
	activeTab().onActivate(ctx_);
}

bool appShell::onEvent(const Event& event) {
	if (event == Event::Custom) {
		applyFindings();
		activeTab().onTick(ctx_);
		return true;
	}
	if (event.is_mouse())
		return true;
	if (not activeTab().capturesInput() and handleGlobalKey(event))
		return true;
	return activeTab().onEvent(event, ctx_);
}

bool appShell::handleGlobalKey(const Event& event) {
	int count = static_cast<int>(tabs_.size());
	if (event == Event::Tab) {
		activate((activeIndex_ + 1) % count);
		return true;
	}
	if (isBackTab(event)) {
		activate((activeIndex_ + count - 1) % count);
		return true;
	}
	for (int position = 0; position < count; ++position) {
		if (not keys::isCharacter(event, static_cast<char>('1' + position)))
			continue;
		activate(position);
		return true;
	}
	// Every change is saved as it is made, so quitting has nothing to write.
	if (keys::isCharacter(event, 'q')) {
		quit_();
		return true;
	}
	return false;
}

Element appShell::render() {
	return vbox({renderTopBar(), activeTab().render(ctx_) | flex, renderBottomBar()});
}

Element appShell::renderTopBar() {
	Elements items;
	for (size_t i = 0; i < tabs_.size(); ++i) {
		iTab& tab = *tabs_[i];
		items.push_back(tabLabel(static_cast<int>(i) + 1, &tab == &activeTab(), tab.title(),
		                         tab.icon(), tab.countBadge(ctx_).value_or("")));
		items.push_back(text("│") | color(theme::kSurface0));
	}
	items.push_back(filler());
	items.push_back(text(fmt::format("v{} ", kVersionString)) | color(theme::kOverlay0));
	return hbox(std::move(items)) | border | color(theme::kSurface0) | size(HEIGHT, EQUAL, 3);
}

Element appShell::renderBottomBar() {
	// The pill, the separators and the borders take about 14 columns; the
	// width is the bar's own, measured on the last frame. Before the first,
	// nothing is left out.
	int width = bottomBox_.x_max - bottomBox_.x_min + 1;
	int room = width > 1 ? width - 14 : std::numeric_limits<int>::max() / 2;

	// The status line comes first (it answers what was just done); hints go
	// in order while they fit beside it, and the last ones are dropped on a
	// narrow terminal.
	int statusGlyphs = static_cast<int>(marquee::splitGlyphs(ctx_.status).size());
	int statusShare = std::max(kMinStatusWidth, std::min(statusGlyphs, room));
	Elements hintItems;
	int hintsWidth = 0;
	for (const auto& item : activeTab().hints()) {
		int itemWidth = static_cast<int>(item.key.size() + item.description.size()) + 3;
		if (hintsWidth + itemWidth > room - statusShare - 2)
			break;
		hintsWidth += itemWidth;
		if (not hintItems.empty())
			hintItems.push_back(text("  "));
		hintItems.push_back(
		    hbox({text(item.key) | bold | color(theme::kText),
		          text(fmt::format(" {}", item.description)) | color(theme::kSubtext)}));
	}

	int maxStatusLength = std::max(kMinStatusWidth, room - hintsWidth - 2);
	return hbox({text(" "), modePill(activeTab().mode()), text(" │ ") | color(theme::kSurface1),
	             text(truncated(ctx_.status, maxStatusLength)) | color(theme::kYellow), filler(),
	             text("  "), hbox(std::move(hintItems)), text(" ")}) |
	       border | color(theme::kSurface0) | size(HEIGHT, EQUAL, 3) | reflect(bottomBox_);
}

void runTui(catalogue& db) {
	auto screen = ScreenInteractive::Fullscreen();
	screen.TrackMouse(false);

	// Web lookups and covers run on the updater's thread; it wakes the loop
	// when something arrives. Declared after the screen, so it stops first.
	bool canConvert = hasProgram("img2sixel");
	updater web(
	    httpGet, httpPost, today,
	    [](const std::string& image, int widthPx, int heightPx) {
		    return runProgram(
		        {"img2sixel", "-w", std::to_string(widthPx), "-h", std::to_string(heightPx)},
		        image);
	    },
	    kRequestPause, [&screen] { screen.PostEvent(Event::Custom); });
	appContext ctx{db, today, httpGet, httpPost, web};
	if (auto cell = terminalCellSize()) {
		ctx.cellWidth = cell->width;
		ctx.cellHeight = cell->height;
	}
	checkFollowed(db, web, today());

	// The two tabs and the shell around them.
	catalogTab anime(mediaType::kAnime);
	catalogTab manga(mediaType::kManga);
	std::vector<iTab*> tabs = {&anime, &manga};
	appShell app(ctx, tabs, screen.ExitLoopClosure());

	// The shell draws everything and routes every key itself; FTXUI only
	// runs the loop.
	auto root = CatchEvent(Renderer([&app] { return app.render(); }),
	                       [&app](Event event) { return app.onEvent(event); });

	// A redraw four times a second scrolls long titles and lets the tabs do
	// their slow work (onTick).
	std::atomic<bool> running{true};
	std::thread ticker([&]() {
		while (running.load()) {
			std::this_thread::sleep_for(kTickInterval);
			screen.PostEvent(Event::Custom);
		}
	});
	// The loop is run one event at a time, so the cover can be drawn over
	// each new frame.
	Loop loop(&screen, root);
	while (not loop.HasQuitted()) {
		ctx.coverProblem = coverProblem(screen, canConvert);
		loop.RunOnceBlocking();
		if (auto picture = app.image())
			drawImage(*picture);
	}
	running = false;
	ticker.join();
}

}  // namespace catalist
