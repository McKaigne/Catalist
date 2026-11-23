#include "cli/refs.hpp"

#include <algorithm>
#include <cctype>
#include <format>

#include "core/text.hpp"

namespace catalist::refs {

std::optional<LocalRef> Parse(const std::string& text) {
	if (text.size() < 3 or text.size() > 11 or text[1] != ':' or
	    (text[0] != 'a' and text[0] != 'm'))
		return std::nullopt;
	if (not std::all_of(text.begin() + 2, text.end(),
	                    [](unsigned char c) { return std::isdigit(c); }))
		return std::nullopt;
	return LocalRef{text[0] == 'a' ? MediaType::kAnime : MediaType::kManga,
	                std::stoi(text.substr(2))};
}

std::string Of(const Entry& named) {
	return Of(named.type, named.id);
}

std::string Of(MediaType type, int id) {
	return std::format("{}:{}", type == MediaType::kAnime ? 'a' : 'm', id);
}

bool TitleHolds(const Entry& shown, const std::string& text) {
	auto words = utf8::Words(text);
	auto holds = [&words](const std::string& name) {
		std::string folded = utf8::Folded(name);
		return std::ranges::all_of(
		    words, [&folded](const auto& word) { return folded.contains(word); });
	};
	return holds(shown.title) or (not shown.english_title.empty() and holds(shown.english_title));
}

std::vector<Entry> Find(const Catalogue& db, const std::string& text,
                        std::optional<MediaType> only) {
	if (auto ref = Parse(text)) {
		auto named = only and *only != ref->type ? std::nullopt : db.GetById(ref->type, ref->id);
		return named ? std::vector<Entry>{*named} : std::vector<Entry>{};
	}
	std::string wanted = utf8::Folded(text);
	std::vector<Entry> exact;
	std::vector<Entry> holding;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& each : db.GetEntries(type)) {
			if (utf8::Folded(each.title) == wanted or utf8::Folded(each.english_title) == wanted)
				exact.push_back(each);
			else if (TitleHolds(each, text))
				holding.push_back(each);
		}
	}
	return exact.empty() ? holding : exact;
}

namespace {

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

std::optional<Entry> Closest(const Catalogue& db, const std::string& text,
                             std::optional<MediaType> only) {
	auto words = utf8::Words(text);
	if (words.empty())
		return std::nullopt;
	std::optional<Entry> closest;
	int lowest = 0;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (only and *only != type)
			continue;
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

}  // namespace catalist::refs
