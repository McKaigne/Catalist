#ifndef CATALIST_TUI_APP_HPP_
#define CATALIST_TUI_APP_HPP_

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "anilist.hpp"
#include "catalogue.hpp"
#include "updater.hpp"

namespace catalist {

// Everything a tab may touch. Built once by runTui; tabs never reach for
// globals. The date and the network are functions so the tests can pin the
// one and fake the other.
struct appContext {
	catalogue& db;
	std::function<isoDate()> today;
	anilist::poster post;
	// Web lookups and covers, fetched in the background.
	updater& web;
	// The message in the bottom bar.
	std::string status = "Ready.";
	// Why covers cannot be shown here, or empty when they can.
	std::string coverProblem{};
	// One terminal cell in pixels, for sizing covers.
	int cellWidth = 10;
	int cellHeight = 20;

	// Writes the catalogue after a change, then shows `done`, or the failure.
	void save(const std::string& done);
};

}  // namespace catalist

namespace catalist {

// Once a day, queues lookups of the ids titles lack.
void startLinking(catalogue& db, updater& web, const isoDate& today);

struct hint {
	std::string key;
	std::string description;
};

// What the keyboard is doing: normal, search, typing a number, adding a title
// or a pending delete. Every mode but normal owns the keyboard.
enum class appMode { kNor, kSch, kEdt, kAdd, kDel };

class iTab {
public:
	virtual ~iTab() = default;

	[[nodiscard]] virtual std::string title() const = 0;
	[[nodiscard]] virtual std::string icon() const = 0;

	virtual ftxui::Element render(appContext& ctx) = 0;
	virtual bool onEvent(const ftxui::Event& event, appContext& ctx) = 0;
	[[nodiscard]] virtual std::vector<hint> hints() const = 0;

	virtual void onActivate(appContext&) {}
	// Called by the shell's redraw timer, four times a second, while the tab
	// is shown; where slow work (the title search) runs, so the status line
	// has been drawn before it starts.
	virtual void onTick(appContext&) {}
	[[nodiscard]] virtual appMode mode() const { return appMode::kNor; }
	// True in every mode but normal (typing, confirming), so the shell must not
	// treat keys like '1' or 'q' as global shortcuts.
	[[nodiscard]] bool capturesInput() const { return mode() != appMode::kNor; }
	[[nodiscard]] virtual std::optional<std::string> countBadge(appContext&) const {
		return std::nullopt;
	}

	// A sixel picture to draw over the frame at a cell (0-based), which the
	// shell writes to the terminal after every redraw.
	struct overlayImage {
		int column = 0;
		int row = 0;
		std::string sixel;
	};
	[[nodiscard]] virtual std::optional<overlayImage> image() const { return std::nullopt; }
};

}  // namespace catalist

namespace catalist {

// The frame around the tabs: the tab bar on top, the mode, status message and
// key hints at the bottom, and the keys that work in every tab.
class appShell {
public:
	appShell(appContext& ctx, std::vector<iTab*> tabs, std::function<void()> quit);

	[[nodiscard]] int activeIndex() const { return activeIndex_; }
	void activate(int index);

	// Tab, Shift-Tab and the number keys switch tabs and q quits, unless the
	// active tab is taking text; everything else goes to the active tab. The
	// redraw timer's Event::Custom becomes the tab's onTick, and the mouse is
	// ignored: Catalist is driven by the keyboard.
	bool onEvent(const ftxui::Event& event);
	[[nodiscard]] ftxui::Element render();
	// The active tab's picture, drawn over the frame after each redraw.
	[[nodiscard]] std::optional<iTab::overlayImage> image() { return activeTab().image(); }

private:
	[[nodiscard]] iTab& activeTab() { return *tabs_[activeIndex_]; }
	void applyFindings();
	bool handleGlobalKey(const ftxui::Event& event);
	[[nodiscard]] ftxui::Element renderTopBar();
	[[nodiscard]] ftxui::Element renderBottomBar();

	appContext& ctx_;
	std::vector<iTab*> tabs_;
	std::function<void()> quit_;
	int activeIndex_ = 0;
	// Where the bottom bar was drawn last, for fitting its hints.
	ftxui::Box bottomBox_;
};

// Runs the full-screen interface until the user quits.
void runTui(catalogue& db);

}  // namespace catalist

#endif  // CATALIST_TUI_APP_HPP_
