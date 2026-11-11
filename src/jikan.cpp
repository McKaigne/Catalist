#include "jikan.hpp"

#include <fmt/format.h>

#include <cctype>
#include <iomanip>
#include <sstream>

#include "json.hpp"

namespace catalist::url {

namespace {

bool isUnreserved(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) or c == '-' or c == '_' or c == '.' or
	       c == '~';
}

}  // namespace

std::string encode(std::string_view text) {
	std::ostringstream encoded;
	encoded.fill('0');
	encoded << std::hex;
	for (char c : text) {
		if (isUnreserved(c))
			encoded << c;
		else
			encoded << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
	}
	return encoded.str();
}

}  // namespace catalist::url

namespace catalist::jikan {

namespace {

constexpr int kResultLimit = 10;
// Jikan lists an anime's episodes a hundred to a page.
constexpr int kEpisodesPerPage = 100;
const std::string kBaseUrl = "https://api.jikan.moe/v4/";

std::string kindOf(mediaType type) {
	return type == mediaType::kAnime ? "anime" : "manga";
}

// MyAnimeList's statuses: "Finished Airing", "Currently Airing" and "Not yet
// aired" for anime; "Finished", "Publishing", "On Hiatus", "Discontinued"
// and "Not yet published" for manga.
bool hasEnded(const std::string& status) {
	return status.rfind("Finished", 0) == 0 or status == "Discontinued";
}

bool isOngoing(const std::string& status) {
	return status == "Currently Airing" or status == "Publishing" or status == "On Hiatus";
}

using titles = std::vector<title>;

int countOrZero(const json::value& field) {
	auto count = field.asInteger();
	return count and *count > 0 ? static_cast<int>(*count) : 0;
}

// Anime carry a plain year; everything else only the date the run began.
std::string yearOf(const json::value& item, mediaType type) {
	if (auto year = item["year"].asInteger())
		return std::to_string(*year);
	const char* run = type == mediaType::kAnime ? "aired" : "published";
	std::string from = item[run]["from"].asString().value_or("");
	return from.size() >= 4 ? from.substr(0, 4) : "";
}

}  // namespace

std::string searchUrl(mediaType type, const std::string& query) {
	return fmt::format("{}{}?q={}&limit={}", kBaseUrl, kindOf(type), url::encode(query),
	                   kResultLimit);
}

std::string detailsUrl(mediaType type, int malId) {
	return fmt::format("{}{}/{}", kBaseUrl, kindOf(type), malId);
}

std::string episodesUrl(int malId, int page) {
	return fmt::format("{}anime/{}/episodes?page={}", kBaseUrl, malId, page);
}

result<titles> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or not reply.value()["data"].isArray())
		return result<titles>::failure("unexpected reply from Jikan");

	titles found;
	for (const auto& item : reply.value()["data"].items()) {
		title match;
		match.malId = countOrZero(item["mal_id"]);
		match.name = item["title"].asString().value_or("");
		if (match.malId == 0 or match.name.empty())
			continue;
		match.totalUnits = countOrZero(item[type == mediaType::kAnime ? "episodes" : "chapters"]);
		match.totalVolumes = countOrZero(item["volumes"]);
		match.year = yearOf(item, type);
		match.score = item["score"].asNumber().value_or(0.0);
		found.push_back(std::move(match));
	}
	return result<titles>::success(std::move(found));
}

result<details> parseDetails(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or reply.value()["data"].isNull())
		return result<details>::failure("unexpected reply from Jikan");
	const json::value& item = reply.value()["data"];
	std::string status = item["status"].asString().value_or("");

	details found;
	found.facts.totalUnits = countOrZero(item[type == mediaType::kAnime ? "episodes" : "chapters"]);
	found.facts.totalVolumes = countOrZero(item["volumes"]);
	found.ongoing = isOngoing(status);
	if (hasEnded(status))
		found.facts.released = found.facts.totalUnits;
	found.imageUrl = item["images"]["jpg"]["image_url"].asString().value_or("");
	return result<details>::success(found);
}

result<episodePage> parseEpisodes(const std::string& body, const isoDate& today) {
	auto reply = json::parse(body);
	if (not reply.ok() or not reply.value()["data"].isArray())
		return result<episodePage>::failure("unexpected reply from Jikan");
	episodePage page;
	page.lastPage = std::max(1, countOrZero(reply.value()["pagination"]["last_visible_page"]));
	// An episode without an air date is listed because it is out.
	for (const auto& episode : reply.value()["data"].items()) {
		auto aired = isoDate::parse(episode["aired"].asString().value_or(""));
		if (not aired or *aired <= today)
			++page.aired;
	}
	return result<episodePage>::success(page);
}

result<details> lookUp(const fetcher& fetch, mediaType type, int malId, const isoDate& today) {
	auto body = fetch(detailsUrl(type, malId));
	if (not body)
		return result<details>::failure("could not reach Jikan");
	auto found = parseDetails(*body, type);
	if (not found.ok() or type != mediaType::kAnime or not found.value().ongoing)
		return found;

	// Count the aired episodes: the first page says how many pages there are,
	// and every page before the last is full.
	auto first = fetch(episodesUrl(malId, 1));
	auto page =
	    first ? parseEpisodes(*first, today) : result<episodePage>::failure("no episode list");
	if (page.ok() and page.value().lastPage > 1) {
		int last = page.value().lastPage;
		auto lastBody = fetch(episodesUrl(malId, last));
		page = lastBody ? parseEpisodes(*lastBody, today)
		                : result<episodePage>::failure("no episode list");
		if (page.ok())
			page.value().aired += (last - 1) * kEpisodesPerPage;
	}
	if (page.ok())
		found.value().facts.released = page.value().aired;
	return found;
}

result<titles> search(const fetcher& fetch, mediaType type, const std::string& query) {
	auto body = fetch(searchUrl(type, query));
	if (not body)
		return result<titles>::failure("could not reach Jikan");
	return parseSearch(*body, type);
}

}  // namespace catalist::jikan
