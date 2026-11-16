#include "web/mal.hpp"

#include <fmt/format.h>

#include "web/json.hpp"

namespace catalist::mal {

namespace {

std::string kindOf(mediaType type) {
	return type == mediaType::kAnime ? "anime" : "manga";
}

std::string fieldsOf(mediaType type) {
	return type == mediaType::kAnime ? "num_episodes,start_date,mean,status"
	                                 : "num_chapters,num_volumes,start_date,mean,status";
}

int countOrZero(const json::value& field) {
	auto count = field.asInteger();
	return count and *count > 0 ? static_cast<int>(*count) : 0;
}

web::match matchOf(const json::value& node, mediaType type) {
	web::match found;
	found.malId = countOrZero(node["id"]);
	found.name = node["title"].asString().value_or("");
	found.totalUnits =
	    countOrZero(node[type == mediaType::kAnime ? "num_episodes" : "num_chapters"]);
	found.totalVolumes = countOrZero(node["num_volumes"]);
	found.year = node["start_date"].asString().value_or("").substr(0, 4);
	found.score = node["mean"].asNumber().value_or(0.0);
	return found;
}

}  // namespace

std::string searchUrl(mediaType type, const std::string& query) {
	return fmt::format("{}/{}?q={}&limit=10&fields={}", kApiUrl, kindOf(type),
	                   web::urlEncoded(query), fieldsOf(type));
}

result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& data = reply.ok() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return result<std::vector<web::match>>::failure("unexpected reply from MyAnimeList");
	std::vector<web::match> found;
	for (const auto& item : data.items()) {
		auto match = matchOf(item["node"], type);
		if (match.malId > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return result<std::vector<web::match>>::success(std::move(found));
}

std::string titleUrl(mediaType type, int malId) {
	return fmt::format("{}/{}/{}?fields={}", kApiUrl, kindOf(type), malId, fieldsOf(type));
}

// The statuses: finished_airing, currently_airing and not_yet_aired for an
// anime; finished, currently_publishing, on_hiatus, discontinued and
// not_yet_published for a manga.
result<web::details> parseTitle(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or countOrZero(reply.value()["id"]) == 0)
		return result<web::details>::failure("MyAnimeList does not know the title");
	const json::value& node = reply.value();
	auto match = matchOf(node, type);
	std::string status = node["status"].asString().value_or("");
	web::details found;
	found.name = match.name;
	found.facts = {match.totalUnits, match.totalVolumes, 0, 0, match.malId, 0};
	found.ongoing =
	    status == "currently_airing" or status == "currently_publishing" or status == "on_hiatus";
	if (status == "finished_airing" or status == "finished" or status == "discontinued")
		found.facts.released = match.totalUnits;
	return result<web::details>::success(std::move(found));
}

std::string tracker::cannotAsk() const {
	if (not net_.malClientId.empty())
		return "";
	return "MyAnimeList answers only programs with a client id: register one at "
	       "https://myanimelist.net/apiconfig and set CATALIST_MAL_CLIENT_ID";
}

std::optional<std::string> tracker::get(const std::string& url) const {
	return net_.fetch({"GET", url, {"X-MAL-CLIENT-ID: " + net_.malClientId}, ""});
}

result<std::vector<web::match>> tracker::search(mediaType type, const std::string& query) {
	if (auto why = cannotAsk(); not why.empty())
		return result<std::vector<web::match>>::failure(why);
	auto body = get(searchUrl(type, query));
	if (not body)
		return result<std::vector<web::match>>::failure("could not reach MyAnimeList");
	return parseSearch(*body, type);
}

result<web::details> tracker::lookUp(const entry& known) {
	if (auto why = cannotAsk(); not why.empty())
		return result<web::details>::failure(why);
	if (known.malId <= 0)
		return result<web::details>::failure("the title has no id to look up");
	auto body = get(titleUrl(known.type, known.malId));
	if (not body)
		return result<web::details>::failure("could not reach MyAnimeList");
	return parseTitle(*body, known.type);
}

}  // namespace catalist::mal
