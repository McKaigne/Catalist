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

}  // namespace catalist::refs
