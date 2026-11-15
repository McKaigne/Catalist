#include "tui/tuiHarness.hpp"

#include <cctype>
#include <ftxui/screen/screen.hpp>

namespace catalist::test {

tuiHarness::tuiHarness()
    : db(tempDir() / "data"),
      updates(
          web.fetcher(), web.poster(), testToday,
          [](const std::string& image, int widthPx, int heightPx) {
	          return std::optional<std::string>("SIXEL[" + image + " " + std::to_string(widthPx) +
	                                            "x" + std::to_string(heightPx) + "]");
          },
          std::chrono::milliseconds(0)),
      ctx{db, testToday, web.fetcher(), web.poster(), updates} {}

std::filesystem::path tuiHarness::dataDir() const {
	return tempDir() / "data";
}

bool tuiHarness::press(iTab& tab, const ftxui::Event& event) {
	return tab.onEvent(event, ctx);
}

bool tuiHarness::type(iTab& tab, const std::string& keys) {
	bool handledAll = true;
	for (char key : keys)
		handledAll = press(tab, ftxui::Event::Character(key)) and handledAll;
	return handledAll;
}

std::string tuiHarness::screen(iTab& tab) {
	return draw(tab.render(ctx));
}

std::string draw(const ftxui::Element& element, int width, int height) {
	auto canvas =
	    ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
	ftxui::Render(canvas, element);
	return canvas.ToString();
}

std::string plain(const std::string& screenText) {
	std::string out;
	for (size_t i = 0; i < screenText.size(); ++i) {
		if (screenText[i] != '\x1b') {
			out += screenText[i];
			continue;
		}
		while (i < screenText.size() and
		       not std::isalpha(static_cast<unsigned char>(screenText[i])))
			++i;
	}
	return out;
}

std::string valueAfter(const std::string& screenText, const std::string& label) {
	std::string text = plain(screenText);
	size_t at = text.find(label);
	if (at == std::string::npos)
		return "";
	at = text.find_first_not_of(' ', at + label.size());
	size_t end = text.find_first_of(" \n", at);
	return at == std::string::npos ? "" : text.substr(at, end - at);
}

int lineOf(const std::string& screenText, const std::string& needle) {
	auto found = screenText.find(needle);
	if (found == std::string::npos)
		return -1;
	int line = 0;
	for (size_t i = 0; i < found; ++i)
		line += screenText[i] == '\n' ? 1 : 0;
	return line;
}

}  // namespace catalist::test
