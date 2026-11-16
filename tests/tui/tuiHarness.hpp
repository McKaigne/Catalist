#ifndef CATALIST_TESTS_TUI_TUI_HARNESS_HPP_
#define CATALIST_TESTS_TUI_TUI_HARNESS_HPP_

#include <filesystem>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "support/testSupport.hpp"
#include "tui/app.hpp"

namespace catalist::test {

// A complete appContext for tab tests: the catalogue in a temporary
// directory, the fixed test date and time, a web that is offline until told
// otherwise, and an updater over it that does not pause and turns images
// into "SIXEL[...]" text.
struct tuiHarness : tempDirFixture {
	tuiHarness();

	bool press(iTab& tab, const ftxui::Event& event);
	bool type(iTab& tab, const std::string& keys);
	[[nodiscard]] std::string screen(iTab& tab);
	[[nodiscard]] std::filesystem::path dataDir() const;

	fakeWeb web;
	catalogue db;
	updater updates;
	appContext ctx;
};

// Draws an element on a screen of that size (200x50 by default) and returns
// the text.
[[nodiscard]] std::string draw(const ftxui::Element& element, int width = 200, int height = 50);

// Screen text without the colour codes, as a person would read it.
[[nodiscard]] std::string plain(const std::string& screenText);

// The word printed after `label` on the same line (spaces skipped), or "".
[[nodiscard]] std::string valueAfter(const std::string& screenText, const std::string& label);

// The screen line on which `needle` first appears, or -1 when it is not shown.
[[nodiscard]] int lineOf(const std::string& screenText, const std::string& needle);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_TUI_TUI_HARNESS_HPP_
