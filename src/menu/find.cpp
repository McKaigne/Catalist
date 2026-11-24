#include "menu/find.hpp"

#include <algorithm>

#include "core/text.hpp"

namespace catalist {

namespace {

// The edits from the words to the name, or nothing when a word is too far from
// all of the name's words to be a typo of any.
std::optional<int> TypoCost(const std::vector<std::string>& words, const std::string& name) {
	auto parts = utf8::Words(name);
	int total = 0;
	for (const auto& word : words) {
		int allowed = static_cast<int>(utf8::SplitGlyphs(word).size()) / 3;
		int best = allowed + 1;
		for (const auto& part : parts)
			best = std::min(best, part.contains(word) ? 0 : utf8::EditDistance(word, part));
		if (best > allowed)
			return std::nullopt;
		total += best;
	}
	return total;
}

}  // namespace

bool TitleHolds(const Entry& shown, const std::string& text) {
	auto words = utf8::Words(text);
	auto holds = [&words](const std::string& name) {
		std::string folded = utf8::Folded(name);
		return std::ranges::all_of(
		    words, [&folded](const auto& word) { return folded.contains(word); });
	};
	return holds(shown.title) or (not shown.english_title.empty() and holds(shown.english_title));
}

std::vector<Entry> FindTitles(const Catalogue& db, const std::string& text) {
	std::string wanted = utf8::Folded(text);
	std::vector<Entry> exact;
	std::vector<Entry> holding;
	if (wanted.empty())
		return holding;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		for (const auto& each : db.GetEntries(type)) {
			if (utf8::Folded(each.title) == wanted or utf8::Folded(each.english_title) == wanted)
				exact.push_back(each);
			else if (TitleHolds(each, text))
				holding.push_back(each);
		}
	}
	return exact.empty() ? holding : exact;
}

std::optional<Entry> ClosestTitle(const Catalogue& db, const std::string& text) {
	auto words = utf8::Words(text);
	if (words.empty())
		return std::nullopt;
	std::optional<Entry> closest;
	int lowest = 0;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		for (const auto& each : db.GetEntries(type)) {
			for (const auto& name : {each.title, each.english_title}) {
				auto cost = name.empty() ? std::nullopt : TypoCost(words, name);
				if (cost and (not closest or *cost < lowest)) {
					closest = each;
					lowest = *cost;
				}
			}
		}
	}
	return closest;
}

}  // namespace catalist
