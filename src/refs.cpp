#include "refs.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>

#include "text.hpp"

namespace catalist::refs {

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

}  // namespace catalist::refs
