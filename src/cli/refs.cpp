#include "cli/refs.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <sstream>

#include "cli/args.hpp"
#include "core/text.hpp"

namespace catalist::refs {

namespace {

std::vector<std::string> SplitWords(const std::string& text) {
	std::vector<std::string> words;
	std::istringstream in(text);
	for (std::string word; in >> word;)
		words.push_back(word);
	return words;
}

// `count` words from `at`, with a space between them.
std::string JoinWords(const std::vector<std::string>& words, size_t at, size_t count) {
	std::string joined;
	for (size_t i = at; i < at + count; ++i)
		joined += (i > at ? " " : "") + words[i];
	return joined;
}

}  // namespace

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
	return utf8::Lowercase(shown.title).find(utf8::Lowercase(text)) != std::string::npos;
}

std::vector<Entry> Find(const Catalogue& db, const std::string& text,
                        std::optional<MediaType> only) {
	if (auto ref = Parse(text)) {
		auto named = only and *only != ref->type ? std::nullopt : db.GetById(ref->type, ref->id);
		return named ? std::vector<Entry>{*named} : std::vector<Entry>{};
	}
	std::string wanted = utf8::Lowercase(text);
	std::vector<Entry> exact;
	std::vector<Entry> holding;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& each : db.GetEntries(type)) {
			if (utf8::Lowercase(each.title) == wanted)
				exact.push_back(each);
			else if (TitleHolds(each, text))
				holding.push_back(each);
		}
	}
	return exact.empty() ? holding : exact;
}

std::optional<Entry> Closest(const Catalogue& db, const std::string& text,
                             std::optional<MediaType> only) {
	if (text.empty() or text[0] == '-' or Parse(text))
		return std::nullopt;
	std::string wanted = utf8::Lowercase(text);
	size_t wanted_words = SplitWords(wanted).size();
	// A typo is a few edits: two, or one in four letters of a longer text.
	size_t best = std::max<size_t>(2, wanted.size() / 4) + 1;
	std::optional<Entry> found;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& each : db.GetEntries(type)) {
			std::string title = utf8::Lowercase(each.title);
			auto words = SplitWords(title);
			std::vector<std::string> tried = {title};
			for (size_t at = 0; at + wanted_words <= words.size(); ++at)
				tried.push_back(JoinWords(words, at, wanted_words));
			for (const auto& candidate : tried) {
				size_t distance = args::EditDistance(wanted, candidate);
				if (distance < best and distance < wanted.size()) {
					best = distance;
					found = each;
				}
			}
		}
	}
	return found;
}

}  // namespace catalist::refs
