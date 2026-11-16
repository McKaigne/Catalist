#include "web/kitsu.hpp"

#include <fmt/format.h>

#include <cstdlib>

#include "web/json.hpp"

namespace catalist::kitsu {

namespace {

std::string kindOf(mediaType type) {
	return type == mediaType::kAnime ? "anime" : "manga";
}

// Kitsu writes ids, and its average rating, as strings: "46474", "84.53".
int idOf(const json::value& field) {
	auto text = field.asString();
	int id = text ? std::atoi(text->c_str()) : 0;
	return id > 0 ? id : 0;
}

int countOrZero(const json::value& field) {
	auto count = field.asInteger();
	return count and *count > 0 ? static_cast<int>(*count) : 0;
}

// The MyAnimeList and AniList ids among the mappings a title names, found in
// the reply's `included`.
void readMappings(const json::value& item, const json::value& included, mediaType type, int& malId,
                  int& anilistId) {
	std::string kind = kindOf(type);
	for (const auto& wanted : item["relationships"]["mappings"]["data"].items()) {
		for (const auto& mapping : included.items()) {
			bool named = mapping["type"].asString().value_or("") == "mappings" and
			             mapping["id"].asString() == wanted["id"].asString();
			if (not named)
				continue;
			std::string site = mapping["attributes"]["externalSite"].asString().value_or("");
			int id = idOf(mapping["attributes"]["externalId"]);
			if (site == "myanimelist/" + kind)
				malId = id;
			else if (site == "anilist/" + kind)
				anilistId = id;
		}
	}
}

int unitsOf(const json::value& attributes, mediaType type) {
	return countOrZero(attributes[type == mediaType::kAnime ? "episodeCount" : "chapterCount"]);
}

web::match matchOf(const json::value& item, const json::value& included, mediaType type) {
	const json::value& attributes = item["attributes"];
	web::match found;
	found.kitsuId = idOf(item["id"]);
	found.name = attributes["canonicalTitle"].asString().value_or("");
	found.totalUnits = unitsOf(attributes, type);
	found.totalVolumes = countOrZero(attributes["volumeCount"]);
	found.year = attributes["startDate"].asString().value_or("").substr(0, 4);
	auto rating = attributes["averageRating"].asString();
	found.score = rating ? std::atof(rating->c_str()) / 10.0 : 0.0;
	readMappings(item, included, type, found.malId, found.anilistId);
	return found;
}

}  // namespace

std::string searchUrl(mediaType type, const std::string& query) {
	return fmt::format("{}/{}?filter[text]={}&page[limit]=10&include=mappings", kApiUrl,
	                   kindOf(type), web::urlEncoded(query));
}

result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& data = reply.ok() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return result<std::vector<web::match>>::failure("unexpected reply from Kitsu");
	std::vector<web::match> found;
	for (const auto& item : data.items()) {
		auto match = matchOf(item, reply.value()["included"], type);
		if (match.kitsuId > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return result<std::vector<web::match>>::success(std::move(found));
}

std::string titleUrl(mediaType type, int kitsuId) {
	return fmt::format("{}/{}/{}?include=mappings", kApiUrl, kindOf(type), kitsuId);
}

// Kitsu's statuses: current, finished, tba, unreleased and upcoming.
result<web::details> parseTitle(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or reply.value()["data"]["id"].isNull())
		return result<web::details>::failure("Kitsu does not know the title");
	const json::value& item = reply.value()["data"];
	auto match = matchOf(item, reply.value()["included"], type);
	std::string status = item["attributes"]["status"].asString().value_or("");
	web::details found;
	found.name = match.name;
	found.facts = {match.totalUnits, match.totalVolumes, 0,
	               match.anilistId,  match.malId,        match.kitsuId};
	found.ongoing = status == "current";
	if (status == "finished")
		found.facts.released = match.totalUnits;
	return result<web::details>::success(std::move(found));
}

std::string mappingUrl(mediaType type, int malId) {
	return fmt::format(
	    "{}/mappings?filter[externalSite]=myanimelist/{}&filter[externalId]={}&include=item",
	    kApiUrl, kindOf(type), malId);
}

int parseMapping(const std::string& body) {
	auto reply = json::parse(body);
	if (not reply.ok())
		return 0;
	for (const auto& mapping : reply.value()["data"].items())
		return idOf(mapping["relationships"]["item"]["data"]["id"]);
	return 0;
}

std::optional<std::string> tracker::get(const std::string& url) const {
	return net_.fetch({"GET", url, {"Accept: application/vnd.api+json"}, ""});
}

result<std::vector<web::match>> tracker::search(mediaType type, const std::string& query) {
	auto body = get(searchUrl(type, query));
	if (not body)
		return result<std::vector<web::match>>::failure("could not reach Kitsu");
	return parseSearch(*body, type);
}

result<web::details> tracker::lookUp(const entry& known) {
	int id = known.kitsuId;
	if (id <= 0 and known.malId > 0) {
		auto mapped = get(mappingUrl(known.type, known.malId));
		if (not mapped)
			return result<web::details>::failure("could not reach Kitsu");
		id = parseMapping(*mapped);
		if (id <= 0)
			return result<web::details>::failure("Kitsu does not know the title");
		net_.pause();
	}
	if (id <= 0)
		return result<web::details>::failure("the title has no id to look up");
	auto body = get(titleUrl(known.type, id));
	if (not body)
		return result<web::details>::failure("could not reach Kitsu");
	return parseTitle(*body, known.type);
}

}  // namespace catalist::kitsu
