#include "menu/input.hpp"

#include <algorithm>
#include <cctype>
#include <format>

#include "core/text.hpp"
#include "menu/output.hpp"

namespace catalist {

namespace {

std::string Trimmed(const std::string& text) {
	auto first = text.find_first_not_of(" \t\r");
	if (first == std::string::npos)
		return "";
	return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

}  // namespace

std::optional<std::string> ReadLine(App& app, const std::string& prompt) {
	app.out << prompt << std::flush;
	std::string answer;
	if (not std::getline(app.in, answer)) {
		app.out << "\n";
		return std::nullopt;
	}
	return Trimmed(answer);
}

std::optional<std::string> AskText(App& app, const std::string& prompt) {
	auto answer = ReadLine(app, prompt);
	return answer and not answer->empty() ? answer : std::nullopt;
}

bool AskYes(App& app, const std::string& question) {
	auto answer = ReadLine(app, std::format("{} {} Yes  {} No: ", question,
	                                        output::Paint(output::LetterHint('y'), output::kCyan,
	                                                      app.color),
	                                        output::Paint(output::LetterHint('n'), output::kCyan,
	                                                      app.color)));
	if (not answer)
		return false;
	std::string word = utf8::Lowercase(*answer);
	return word == "y" or word == "yes";
}

std::optional<int> ParseNumber(const std::string& text) {
	bool digits = not text.empty() and text.size() <= 6 and
	              std::all_of(text.begin(), text.end(),
	                          [](unsigned char c) { return std::isdigit(c); });
	return digits ? std::optional<int>(std::stoi(text)) : std::nullopt;
}

int ReadChoice(App& app, int highest) {
	while (true) {
		auto answer = ReadLine(
		    app, std::format("{} Enter choice [{}-{}]: ",
		                     output::Paint(output::icon::kPrompt, output::kMagenta, app.color),
		                     output::NumberHint(0), output::NumberHint(highest)));
		if (not answer)
			return 0;
		auto number = ParseNumber(*answer);
		if (number and *number <= highest)
			return *number;
		app.out << std::format("{} Enter a number from 0 to {}.\n",
		                       output::Paint(output::icon::kError, output::kYellow, app.color),
		                       highest);
	}
}

void Pause(App& app) {
	(void)ReadLine(app, output::Paint("\nPress Enter to continue...", output::kGray, app.color));
}

}  // namespace catalist
