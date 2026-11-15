#include "anilist.hpp"

#include <fmt/format.h>

#include <algorithm>

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
details detailsOf(const json::value& media, mediaType type) {
	std::string status = media["status"].asString().value_or("");
	details found;
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
	const json::value& cover = media["coverImage"];
	found.imageUrl =
	    cover["extraLarge"].asString().value_or(cover["large"].asString().value_or(""));
	return found;
}

}  // namespace

titleRef refOf(const entry& e) {
	return {e.type, e.anilistId, e.malId};
}

std::string lookupRequest(const titleRef& wanted) {
	// AniList's own id when known; a MyAnimeList id goes in idMal.
	bool own = wanted.anilistId > 0;
	return fmt::format(R"({{"query":"query($id:Int,$type:MediaType){{Media({}:$id,type:$type){{)"
	                   R"(id idMal title{{romaji}} episodes chapters volumes status )"
	                   R"(nextAiringEpisode{{episode}} coverImage{{large extraLarge}}}}}}",)"
	                   R"("variables":{{"id":{},"type":"{}"}}}})",
	                   own ? "id" : "idMal", own ? wanted.anilistId : wanted.malId,
	                   typeName(wanted.type));
}

result<details> parseLookup(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or reply.value()["data"]["Media"].isNull())
		return result<details>::failure("AniList does not know the title");
	return result<details>::success(detailsOf(reply.value()["data"]["Media"], type));
}

std::string searchRequest(mediaType type, const std::string& query) {
	return fmt::format(
	    R"({{"query":"query($search:String,$type:MediaType){{Page(perPage:10){{)"
	    R"(media(search:$search,type:$type,sort:SEARCH_MATCH){{id idMal title{{romaji}} )"
	    R"(episodes chapters volumes averageScore startDate{{year}}}}}}}}",)"
	    R"("variables":{{"search":{},"type":"{}"}}}})",
	    jsonString(query), typeName(type));
}

result<std::vector<title>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& media = reply.ok() ? reply.value()["data"]["Page"]["media"] : json::value();
	if (not media.isArray())
		return result<std::vector<title>>::failure("unexpected reply from AniList");
	std::vector<title> found;
	for (const auto& item : media.items()) {
		title match;
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
	return result<std::vector<title>>::success(std::move(found));
}

std::string linkRequest(mediaType type, provider known, const std::vector<int>& ids) {
	std::string list;
	for (int id : ids)
		list += list.empty() ? std::to_string(id) : fmt::format(",{}", id);
	return fmt::format(
	    R"({{"query":"query($ids:[Int],$type:MediaType){{Page(perPage:50){{)"
	    R"(media({}:$ids,type:$type){{id idMal}}}}}}","variables":{{"ids":[{}],"type":"{}"}}}})",
	    known == provider::kAniList ? "id_in" : "idMal_in", list, typeName(type));
}

result<std::vector<idPair>> parseLink(const std::string& body) {
	auto reply = json::parse(body);
	const json::value& media = reply.ok() ? reply.value()["data"]["Page"]["media"] : json::value();
	if (not media.isArray())
		return result<std::vector<idPair>>::failure("unexpected reply from AniList");
	std::vector<idPair> found;
	for (const auto& item : media.items())
		found.push_back({countOrZero(item["id"]), countOrZero(item["idMal"])});
	return result<std::vector<idPair>>::success(std::move(found));
}

result<std::vector<idPair>> linkIds(const poster& post, mediaType type, provider known,
                                    const std::vector<int>& ids) {
	constexpr size_t kPerRequest = 50;
	std::vector<idPair> found;
	for (size_t start = 0; start < ids.size(); start += kPerRequest) {
		std::vector<int> batch(
		    ids.begin() + static_cast<long>(start),
		    ids.begin() + static_cast<long>(std::min(ids.size(), start + kPerRequest)));
		auto body = post(kApiUrl, linkRequest(type, known, batch));
		if (not body)
			return result<std::vector<idPair>>::failure("could not reach AniList");
		auto page = parseLink(*body);
		if (not page.ok())
			return page;
		found.insert(found.end(), page.value().begin(), page.value().end());
	}
	return result<std::vector<idPair>>::success(std::move(found));
}

result<details> lookUp(const poster& post, const titleRef& wanted) {
	if (wanted.anilistId <= 0 and wanted.malId <= 0)
		return result<details>::failure("the title has no id to look up");
	auto body = post(kApiUrl, lookupRequest(wanted));
	if (not body)
		return result<details>::failure("could not reach AniList");
	return parseLookup(*body, wanted.type);
}

result<std::vector<title>> search(const poster& post, mediaType type, const std::string& query) {
	auto body = post(kApiUrl, searchRequest(type, query));
	if (not body)
		return result<std::vector<title>>::failure("could not reach AniList");
	return parseSearch(*body, type);
}

}  // namespace catalist::anilist
