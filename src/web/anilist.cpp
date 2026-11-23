#include "web/anilist.hpp"

#include <format>

#include "web/json.hpp"

namespace catalist::anilist {

namespace {

std::string TypeName(MediaType type) {
	return type == MediaType::kAnime ? "ANIME" : "MANGA";
}

// A JSON string literal: quotes, backslashes and control characters escaped.
std::string JsonString(const std::string& text) {
	std::string quoted = "\"";
	for (unsigned char c : text) {
		if (c == '"' or c == '\\')
			quoted += std::format("\\{}", static_cast<char>(c));
		else if (c < 0x20)
			quoted += std::format("\\u{:04x}", c);
		else
			quoted += static_cast<char>(c);
	}
	return quoted + "\"";
}

Details DetailsOf(const json::Value& media, MediaType type) {
	Details found;
	found.name = media["title"]["romaji"].AsString().value_or("");
	found.facts.anilist_id = media["id"].AsCount();
	found.facts.mal_id = media["idMal"].AsCount();
	found.facts.total_units = media[type == MediaType::kAnime ? "episodes" : "chapters"].AsCount();
	found.facts.total_volumes = media["volumes"].AsCount();
	return found;
}

// The body of AniList's reply to a query, or what to tell the person.
Result<std::string> Ask(const web::Fetcher& fetch, const std::string& query) {
	auto got =
	    fetch({"POST", kApiUrl, {"Content-Type: application/json", "Accept: application/json"}, query});
	if (not got)
		return std::unexpected("could not reach AniList; check the connection and try again");
	int status = got->status;
	if (status >= 200 and status < 300)
		return got->body;
	if (status == 429 or status >= 500)
		return std::unexpected(std::format(
		    "AniList is busy or down (HTTP {}); check the connection and try again", status));
	if (status == 404)
		return std::unexpected("AniList does not know the title");
	return std::unexpected(std::format("AniList said no (HTTP {})", status));
}

}  // namespace

std::string LookupRequest(const TitleRef& wanted) {
	// AniList's own id when known; a MyAnimeList id goes in idMal.
	bool own = wanted.anilist_id > 0;
	return std::format(R"({{"query":"query($id:Int,$type:MediaType){{Media({}:$id,type:$type){{)"
	                   R"(id idMal title{{romaji}} episodes chapters volumes}}}}",)"
	                   R"("variables":{{"id":{},"type":"{}"}}}})",
	                   own ? "id" : "idMal", own ? wanted.anilist_id : wanted.mal_id,
	                   TypeName(wanted.type));
}

Result<Details> ParseLookup(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	if (not reply.has_value() or reply.value()["data"]["Media"].IsNull())
		return std::unexpected("AniList does not know the title");
	return DetailsOf(reply.value()["data"]["Media"], type);
}

std::string SearchRequest(MediaType type, const std::string& query) {
	return std::format(
	    R"({{"query":"query($search:String,$type:MediaType){{Page(perPage:10){{)"
	    R"(media(search:$search,type:$type,sort:SEARCH_MATCH){{id idMal title{{romaji}} )"
	    R"(episodes chapters volumes averageScore startDate{{year}}}}}}}}",)"
	    R"("variables":{{"search":{},"type":"{}"}}}})",
	    JsonString(query), TypeName(type));
}

Result<std::vector<Match>> ParseSearch(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	const json::Value& media =
	    reply.has_value() ? reply.value()["data"]["Page"]["media"] : json::Value();
	if (not media.IsArray())
		return std::unexpected("unexpected reply from AniList");
	std::vector<Match> found;
	for (const auto& item : media.Items()) {
		Match match;
		match.anilist_id = item["id"].AsCount();
		match.mal_id = item["idMal"].AsCount();
		match.name = item["title"]["romaji"].AsString().value_or("");
		if (match.anilist_id == 0 or match.name.empty())
			continue;
		match.total_units = item[type == MediaType::kAnime ? "episodes" : "chapters"].AsCount();
		match.total_volumes = item["volumes"].AsCount();
		int year = item["startDate"]["year"].AsCount();
		match.year = year > 0 ? std::to_string(year) : "";
		match.score = item["averageScore"].AsNumber().value_or(0.0) / 10.0;
		found.push_back(std::move(match));
	}
	return found;
}

Result<std::vector<Match>> Search(const web::Fetcher& fetch, MediaType type,
                                  const std::string& query) {
	auto body = Ask(fetch, SearchRequest(type, query));
	if (not body)
		return std::unexpected(body.error());
	return ParseSearch(*body, type);
}

Result<Details> LookUp(const web::Fetcher& fetch, const Entry& known) {
	if (known.anilist_id <= 0 and known.mal_id <= 0)
		return std::unexpected("the title has no id to look up");
	auto body = Ask(fetch, LookupRequest({known.type, known.anilist_id, known.mal_id}));
	if (not body)
		return std::unexpected(body.error());
	return ParseLookup(*body, known.type);
}

}  // namespace catalist::anilist
