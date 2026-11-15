#include "tui/style.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>

#include "text.hpp"

namespace catalist {

std::string statusToIcon(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return icon::kWatching;
		case watchStatus::kCompleted:
			return icon::kCompleted;
		case watchStatus::kOnHold:
			return icon::kOnHold;
		case watchStatus::kDropped:
			return icon::kDropped;
		case watchStatus::kPlanToWatch:
			return icon::kPlan;
	}
	return icon::kPlan;
}

ftxui::Color statusToColor(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return theme::kGreen;
		case watchStatus::kCompleted:
			return theme::kMauve;
		case watchStatus::kOnHold:
			return theme::kYellow;
		case watchStatus::kDropped:
			return theme::kRed;
		case watchStatus::kPlanToWatch:
			return theme::kOverlay0;
	}
	return theme::kSubtext;
}

std::string typeToIcon(mediaType type) {
	switch (type) {
		case mediaType::kAnime:
			return icon::kAnime;
		case mediaType::kManga:
			return icon::kManga;
	}
	return icon::kAnime;
}

ftxui::Color typeToColor(mediaType type) {
	switch (type) {
		case mediaType::kAnime:
			return theme::kBlue;
		case mediaType::kManga:
			return theme::kPeach;
	}
	return theme::kBlue;
}

std::string renderStarMeter(int score) {
	score = std::clamp(score, 0, 10);
	std::string meter;
	// Each of the five stars stands for two points.
	for (int star = 0; star < 5; ++star) {
		int points = score - star * 2;
		meter += star > 0 ? " " : "";
		meter += points >= 2 ? icon::kStarFull : points == 1 ? icon::kStarHalf : icon::kStarEmpty;
	}
	return meter;
}

}  // namespace catalist

namespace catalist {

using namespace ftxui;

Element pill(const std::string& label, Color fg, Color bg, bool boldText) {
	static const std::string kCapLeft = "\xEE\x82\xB6";
	static const std::string kCapRight = "\xEE\x82\xB4";
	auto content = text(label) | color(fg) | bgcolor(bg);
	if (boldText)
		content = content | bold;
	return hbox({text(kCapLeft) | color(bg), content, text(kCapRight) | color(bg)});
}

}  // namespace catalist

namespace catalist::marquee {

namespace {

uint64_t steadyNowMs() {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
	                                 std::chrono::steady_clock::now().time_since_epoch())
	                                 .count());
}

}  // namespace

std::string scroll(const std::string& text, size_t maxGlyphs, uint64_t elapsedMs, uint64_t pauseMs,
                   uint64_t stepMs) {
	if (utf8::splitGlyphs(text).size() <= maxGlyphs)
		return text;

	auto loop = utf8::splitGlyphs(fmt::format("{}   •   ", text));
	uint64_t cycleMs = pauseMs + loop.size() * stepMs;
	uint64_t cycleTime = elapsedMs % cycleMs;
	size_t offset =
	    cycleTime < pauseMs ? 0 : static_cast<size_t>((cycleTime - pauseMs) / stepMs) % loop.size();

	std::string window;
	for (size_t i = 0; i < maxGlyphs; ++i)
		window += loop[(offset + i) % loop.size()];
	return window;
}

uint64_t selectionTimer::elapsedFor(int selectedIndex) {
	uint64_t now = steadyNowMs();
	if (trackedIndex_ != selectedIndex) {
		trackedIndex_ = selectedIndex;
		changedAtMs_ = now;
	}
	return now >= changedAtMs_ ? now - changedAtMs_ : 0;
}

}  // namespace catalist::marquee

namespace catalist::keys {

using ftxui::Event;

bool isMoveUp(const Event& event) {
	return event == Event::ArrowUp or isCharacter(event, 'k');
}

bool isMoveDown(const Event& event) {
	return event == Event::ArrowDown or isCharacter(event, 'j');
}

bool isMoveLeft(const Event& event) {
	return event == Event::ArrowLeft or isCharacter(event, 'h');
}

bool isMoveRight(const Event& event) {
	return event == Event::ArrowRight or isCharacter(event, 'l');
}

bool isJumpTop(const Event& event) {
	return event == Event::Home or isCharacter(event, 'g');
}

bool isJumpBottom(const Event& event) {
	return event == Event::End or isCharacter(event, 'G');
}

bool isConfirm(const Event& event) {
	return event == Event::Return;
}

bool isConfirmOrSpace(const Event& event) {
	return event == Event::Return or event == Event::Character(' ');
}

bool isIncrement(const Event& event) {
	return isCharacter(event, '+') or isCharacter(event, '=');
}

bool isCharacter(const Event& event, char expected) {
	return event == Event::Character(std::string(1, expected));
}

}  // namespace catalist::keys

namespace catalist {

bool textField::handle(const ftxui::Event& event) {
	if (event == ftxui::Event::Backspace) {
		auto glyphs = utf8::splitGlyphs(value);
		if (not glyphs.empty())
			value.resize(value.size() - glyphs.back().size());
		return true;
	}
	if (not event.is_character())
		return false;
	value += event.character();
	return true;
}

ftxui::Element textField::render(const std::string& placeholder, bool active) const {
	using namespace ftxui;
	Element shown = value.empty() ? text(placeholder) | color(theme::kOverlay0)
	                              : text(value) | color(active ? theme::kPeach : theme::kText);
	if (not active)
		return shown;
	Element cursor = text(" ") | inverted;
	return value.empty() ? hbox({cursor, shown}) : hbox({shown, cursor});
}

}  // namespace catalist
