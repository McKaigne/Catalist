#include "web/anilist.hpp"

#include <format>

#include "web/json.hpp"

namespace catalist::anilist {

namespace {

std::string TypeName(MediaType type) {
	return type == MediaType::kAnime ? "ANIME" : "MANGA";
}

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

Result<std::string> Ask(const web::Fetcher& fetch, const std::string& query) {
	auto got = fetch(
	    {kApiUrl, {"Content-Type: application/json", "Accept: application/json"}, query});
	if (not got)
		return std::unexpected("could not reach AniList; check the connection and try again");
	int status = got->status;
	if (status >= 200 and status < 300)
		return got->body;
	if (status == 429 or status >= 500)
		return std::unexpected(std::format(
		    "AniList is busy or down (HTTP {}); check the connection and try again", status));
	return std::unexpected(std::format("AniList said no (HTTP {})", status));
}

// AniList's manga formats, MANGA, NOVEL and ONE_SHOT, are not kept.
MediaFormat FormatOf(const std::string& name) {
	const std::pair<const char*, MediaFormat> formats[] = {
	    {"TV", MediaFormat::kTv},       {"TV_SHORT", MediaFormat::kTvShort},
	    {"MOVIE", MediaFormat::kMovie}, {"SPECIAL", MediaFormat::kSpecial},
	    {"OVA", MediaFormat::kOva},     {"ONA", MediaFormat::kOna},
	    {"MUSIC", MediaFormat::kMusic}};
	for (const auto& [key, format] : formats)
		if (name == key)
			return format;
	return MediaFormat::kUnknown;
}

}  // namespace

std::string SearchRequest(std::optional<MediaType> only, const std::string& query) {
	constexpr int kEachOfBoth = 5;
	constexpr int kOfOne = 10;
	std::string pages;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (only and *only != type)
			continue;
		pages += std::format(
		    "{}:Page(perPage:{}){{media(search:$search,type:{},isAdult:false,sort:SEARCH_MATCH){{"
		    "id idMal title{{romaji english}} episodes chapters volumes format averageScore "
		    "startDate{{year}}}}}} ",
		    TypeKey(type), only ? kOfOne : kEachOfBoth, TypeName(type));
	}
	return std::format(R"({{"query":"query($search:String){{{}}}","variables":{{"search":{}}}}})",
	                   pages, JsonString(query));
}

Result<std::vector<Match>> ParseSearch(const std::string& body) {
	auto reply = json::Parse(body);
	if (not reply.has_value())
		return std::unexpected("unexpected reply from AniList");
	const json::Value& data = reply.value()["data"];
	bool answered = false;
	std::vector<Match> found;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		const json::Value& media = data[TypeKey(type)]["media"];
		if (not media.IsArray())
			continue;
		answered = true;
		for (const auto& item : media.Items()) {
			Match match;
			match.type = type;
			match.anilist_id = item["id"].AsCount();
			match.mal_id = item["idMal"].AsCount();
			match.name = item["title"]["romaji"].AsString().value_or("");
			match.english = item["title"]["english"].AsString().value_or("");
			if (match.anilist_id == 0 or match.name.empty())
				continue;
			match.total_units = item[type == MediaType::kAnime ? "episodes" : "chapters"].AsCount();
			match.total_volumes = item["volumes"].AsCount();
			if (type == MediaType::kAnime)
				match.format = FormatOf(item["format"].AsString().value_or(""));
			int year = item["startDate"]["year"].AsCount();
			match.year = year > 0 ? std::to_string(year) : "";
			match.score = item["averageScore"].AsNumber().value_or(0.0) / 10.0;
			found.push_back(std::move(match));
		}
	}
	if (not answered)
		return std::unexpected("unexpected reply from AniList");
	return found;
}

Result<std::vector<Match>> Search(const web::Fetcher& fetch, std::optional<MediaType> only,
                                  const std::string& query) {
	auto body = Ask(fetch, SearchRequest(only, query));
	if (not body)
		return std::unexpected(body.error());
	return ParseSearch(*body);
}

}  // namespace catalist::anilist
