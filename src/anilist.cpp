#include "anilist.hpp"

#include <fmt/format.h>

#include "json.hpp"

namespace catalist::anilist {

namespace {

std::string typeName(mediaType type) {
	return type == mediaType::kAnime ? "ANIME" : "MANGA";
}

// A JSON string literal: quotes, backslashes and control characters escaped.
std::string jsonString(const std::string& text) {
	std::string quoted = "\"";
	for (unsigned char c : text) {
		if (c == '"' or c == '\\')
			quoted += fmt::format("\\{}", static_cast<char>(c));
		else if (c < 0x20)
			quoted += fmt::format("\\u{:04x}", c);
		else
			quoted += static_cast<char>(c);
	}
	return quoted + "\"";
}

int countOrZero(const json::value& field) {
	auto count = field.asInteger();
	return count and *count > 0 ? static_cast<int>(*count) : 0;
}

// AniList's statuses: FINISHED, RELEASING, NOT_YET_RELEASED, CANCELLED and
// HIATUS.
jikan::details detailsOf(const json::value& media, mediaType type) {
	std::string status = media["status"].asString().value_or("");
	jikan::details found;
	found.name = media["title"]["romaji"].asString().value_or("");
	found.facts.totalUnits =
	    countOrZero(media[type == mediaType::kAnime ? "episodes" : "chapters"]);
	found.facts.totalVolumes = countOrZero(media["volumes"]);
	found.ongoing = status == "RELEASING" or status == "HIATUS";
	if (status == "FINISHED" or status == "CANCELLED")
		found.facts.released = found.facts.totalUnits;
	// An airing anime: every episode before the next one to air is out.
	int nextEpisode = countOrZero(media["nextAiringEpisode"]["episode"]);
	if (found.ongoing and nextEpisode > 1)
		found.facts.released = nextEpisode - 1;
	const json::value& cover = media["coverImage"];
	found.imageUrl =
	    cover["extraLarge"].asString().value_or(cover["large"].asString().value_or(""));
	return found;
}

}  // namespace

std::string lookupRequest(mediaType type, int malId) {
	return fmt::format(
	    R"({{"query":"query($id:Int,$type:MediaType){{Media(idMal:$id,type:$type){{)"
	    R"(title{{romaji}} episodes chapters volumes status nextAiringEpisode{{episode}} )"
	    R"(coverImage{{large extraLarge}}}}}}","variables":{{"id":{},"type":"{}"}}}})",
	    malId, typeName(type));
}

result<jikan::details> parseLookup(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or reply.value()["data"]["Media"].isNull())
		return result<jikan::details>::failure("AniList does not know the title");
	return result<jikan::details>::success(detailsOf(reply.value()["data"]["Media"], type));
}

std::string searchRequest(mediaType type, const std::string& query) {
	return fmt::format(
	    R"({{"query":"query($search:String,$type:MediaType){{Page(perPage:10){{)"
	    R"(media(search:$search,type:$type,sort:SEARCH_MATCH){{idMal title{{romaji}} )"
	    R"(episodes chapters volumes averageScore startDate{{year}}}}}}}}",)"
	    R"("variables":{{"search":{},"type":"{}"}}}})",
	    jsonString(query), typeName(type));
}

result<std::vector<jikan::title>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& media = reply.ok() ? reply.value()["data"]["Page"]["media"] : json::value();
	if (not media.isArray())
		return result<std::vector<jikan::title>>::failure("unexpected reply from AniList");
	std::vector<jikan::title> found;
	for (const auto& item : media.items()) {
		jikan::title match;
		match.malId = countOrZero(item["idMal"]);
		match.name = item["title"]["romaji"].asString().value_or("");
		// The catalogue is keyed on MyAnimeList ids; titles without one are
		// left out.
		if (match.malId == 0 or match.name.empty())
			continue;
		match.totalUnits = countOrZero(item[type == mediaType::kAnime ? "episodes" : "chapters"]);
		match.totalVolumes = countOrZero(item["volumes"]);
		int year = countOrZero(item["startDate"]["year"]);
		match.year = year > 0 ? std::to_string(year) : "";
		match.score = item["averageScore"].asNumber().value_or(0.0) / 10.0;
		found.push_back(std::move(match));
	}
	return result<std::vector<jikan::title>>::success(std::move(found));
}

result<jikan::details> lookUp(const poster& post, mediaType type, int malId) {
	auto body = post(kApiUrl, lookupRequest(type, malId));
	if (not body)
		return result<jikan::details>::failure("could not reach AniList");
	return parseLookup(*body, type);
}

result<std::vector<jikan::title>> search(const poster& post, mediaType type,
                                         const std::string& query) {
	auto body = post(kApiUrl, searchRequest(type, query));
	if (not body)
		return result<std::vector<jikan::title>>::failure("could not reach AniList");
	return parseSearch(*body, type);
}

}  // namespace catalist::anilist
