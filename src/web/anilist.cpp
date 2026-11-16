#include "web/anilist.hpp"

#include <fmt/format.h>

#include "web/json.hpp"

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
web::details detailsOf(const json::value& media, mediaType type) {
	std::string status = media["status"].asString().value_or("");
	web::details found;
	found.name = media["title"]["romaji"].asString().value_or("");
	found.facts.anilistId = countOrZero(media["id"]);
	found.facts.malId = countOrZero(media["idMal"]);
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
	return found;
}

}  // namespace

std::string lookupRequest(const titleRef& wanted) {
	// AniList's own id when known; a MyAnimeList id goes in idMal.
	bool own = wanted.anilistId > 0;
	return fmt::format(R"({{"query":"query($id:Int,$type:MediaType){{Media({}:$id,type:$type){{)"
	                   R"(id idMal title{{romaji}} episodes chapters volumes status )"
	                   R"(nextAiringEpisode{{episode}}}}}}",)"
	                   R"("variables":{{"id":{},"type":"{}"}}}})",
	                   own ? "id" : "idMal", own ? wanted.anilistId : wanted.malId,
	                   typeName(wanted.type));
}

result<web::details> parseLookup(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or reply.value()["data"]["Media"].isNull())
		return result<web::details>::failure("AniList does not know the title");
	return result<web::details>::success(detailsOf(reply.value()["data"]["Media"], type));
}

std::string searchRequest(mediaType type, const std::string& query) {
	return fmt::format(
	    R"({{"query":"query($search:String,$type:MediaType){{Page(perPage:10){{)"
	    R"(media(search:$search,type:$type,sort:SEARCH_MATCH){{id idMal title{{romaji}} )"
	    R"(episodes chapters volumes averageScore startDate{{year}}}}}}}}",)"
	    R"("variables":{{"search":{},"type":"{}"}}}})",
	    jsonString(query), typeName(type));
}

result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& media = reply.ok() ? reply.value()["data"]["Page"]["media"] : json::value();
	if (not media.isArray())
		return result<std::vector<web::match>>::failure("unexpected reply from AniList");
	std::vector<web::match> found;
	for (const auto& item : media.items()) {
		web::match match;
		match.anilistId = countOrZero(item["id"]);
		match.malId = countOrZero(item["idMal"]);
		match.name = item["title"]["romaji"].asString().value_or("");
		if (match.anilistId == 0 or match.name.empty())
			continue;
		match.totalUnits = countOrZero(item[type == mediaType::kAnime ? "episodes" : "chapters"]);
		match.totalVolumes = countOrZero(item["volumes"]);
		int year = countOrZero(item["startDate"]["year"]);
		match.year = year > 0 ? std::to_string(year) : "";
		match.score = item["averageScore"].asNumber().value_or(0.0) / 10.0;
		found.push_back(std::move(match));
	}
	return result<std::vector<web::match>>::success(std::move(found));
}

std::optional<std::string> tracker::ask(const std::string& query) const {
	return net_.fetch(
	    {"POST", kApiUrl, {"Content-Type: application/json", "Accept: application/json"}, query});
}

result<web::details> tracker::lookUp(const entry& known) {
	if (known.anilistId <= 0 and known.malId <= 0)
		return result<web::details>::failure("the title has no id to look up");
	auto body = ask(lookupRequest({known.type, known.anilistId, known.malId}));
	if (not body)
		return result<web::details>::failure("could not reach AniList");
	return parseLookup(*body, known.type);
}

result<std::vector<web::match>> tracker::search(mediaType type, const std::string& query) {
	auto body = ask(searchRequest(type, query));
	if (not body)
		return result<std::vector<web::match>>::failure("could not reach AniList");
	return parseSearch(*body, type);
}

}  // namespace catalist::anilist
