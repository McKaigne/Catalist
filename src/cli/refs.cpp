#include "cli/refs.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <sstream>

#include "cli/args.hpp"
#include "core/text.hpp"

namespace catalist::refs {

namespace {

std::vector<std::string> splitWords(const std::string& text) {
	std::vector<std::string> words;
	std::istringstream in(text);
	for (std::string word; in >> word;)
		words.push_back(word);
	return words;
}

// `count` words from `at`, with a space between them.
std::string joinWords(const std::vector<std::string>& words, size_t at, size_t count) {
	std::string joined;
	for (size_t i = at; i < at + count; ++i)
		joined += (i > at ? " " : "") + words[i];
	return joined;
}

}  // namespace

std::optional<localRef> parse(const std::string& text) {
	if (text.size() < 3 or text.size() > 11 or text[1] != ':' or
	    (text[0] != 'a' and text[0] != 'm'))
		return std::nullopt;
	if (not std::all_of(text.begin() + 2, text.end(),
	                    [](unsigned char c) { return std::isdigit(c); }))
		return std::nullopt;
	return localRef{text[0] == 'a' ? mediaType::kAnime : mediaType::kManga,
	                std::stoi(text.substr(2))};
}

std::string of(const entry& named) {
	return of(named.type, named.id);
}

std::string of(mediaType type, int id) {
	return fmt::format("{}:{}", type == mediaType::kAnime ? 'a' : 'm', id);
}

bool titleHolds(const entry& shown, const std::string& text) {
	return utf8::lowercase(shown.title).find(utf8::lowercase(text)) != std::string::npos;
}

std::vector<entry> find(const catalogue& db, const std::string& text,
                        std::optional<mediaType> only) {
	if (auto ref = parse(text)) {
		auto named = only and *only != ref->type ? std::nullopt : db.getById(ref->type, ref->id);
		return named ? std::vector<entry>{*named} : std::vector<entry>{};
	}
	std::string wanted = utf8::lowercase(text);
	std::vector<entry> exact;
	std::vector<entry> holding;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& each : db.getEntries(type)) {
			if (utf8::lowercase(each.title) == wanted)
				exact.push_back(each);
			else if (titleHolds(each, text))
				holding.push_back(each);
		}
	}
	return exact.empty() ? holding : exact;
}

std::optional<entry> closest(const catalogue& db, const std::string& text,
                             std::optional<mediaType> only) {
	if (text.empty() or text[0] == '-' or parse(text))
		return std::nullopt;
	std::string wanted = utf8::lowercase(text);
	size_t wantedWords = splitWords(wanted).size();
	// A typo is a few edits: two, or one in four letters of a longer text.
	size_t best = std::max<size_t>(2, wanted.size() / 4) + 1;
	std::optional<entry> found;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		if (only and *only != type)
			continue;
		for (const auto& each : db.getEntries(type)) {
			std::string title = utf8::lowercase(each.title);
			auto words = splitWords(title);
			std::vector<std::string> tried = {title};
			for (size_t at = 0; at + wantedWords <= words.size(); ++at)
				tried.push_back(joinWords(words, at, wantedWords));
			for (const auto& candidate : tried) {
				size_t distance = args::editDistance(wanted, candidate);
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
