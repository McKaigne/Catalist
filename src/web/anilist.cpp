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

std::string listStatus(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return "CURRENT";
		case watchStatus::kCompleted:
			return "COMPLETED";
		case watchStatus::kOnHold:
			return "PAUSED";
		case watchStatus::kDropped:
			return "DROPPED";
		case watchStatus::kPlanToWatch:
			return "PLANNING";
	}
	return "PLANNING";
}

// A FuzzyDateInput; all null clears the date.
std::string fuzzyDate(const std::optional<isoDate>& day) {
	if (not day)
		return R"({"year":null,"month":null,"day":null})";
	return fmt::format(R"({{"year":{},"month":{},"day":{}}})", day->year(), day->month(),
	                   day->day());
}

std::optional<watchStatus> statusOf(const std::string& listStatus) {
	if (listStatus == "CURRENT" or listStatus == "REPEATING")
		return watchStatus::kWatching;
	if (listStatus == "COMPLETED")
		return watchStatus::kCompleted;
	if (listStatus == "PAUSED")
		return watchStatus::kOnHold;
	if (listStatus == "DROPPED")
		return watchStatus::kDropped;
	if (listStatus == "PLANNING")
		return watchStatus::kPlanToWatch;
	return std::nullopt;
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

std::string saveRequest(const entry& title) {
	// scoreRaw is out of 100 whatever scale the person chose on AniList.
	return fmt::format(
	    R"({{"query":"mutation($m:Int,$s:MediaListStatus,$c:Int,$p:Int,$v:Int,$r:Int,)"
	    R"($a:FuzzyDateInput,$f:FuzzyDateInput){{SaveMediaListEntry(mediaId:$m,status:$s,)"
	    R"(scoreRaw:$c,progress:$p,progressVolumes:$v,repeat:$r,startedAt:$a,completedAt:$f))"
	    R"({{id}}}}",)"
	    R"("variables":{{"m":{},"s":"{}","c":{},"p":{},"v":{},"r":{},"a":{},"f":{}}}}})",
	    title.anilistId, listStatus(title.status), title.score * 10, title.progress, title.volumes,
	    title.rewatched, fuzzyDate(title.startDate), fuzzyDate(title.finishDate));
}

std::string listEntryRequest(int mediaId, const std::string& userId) {
	return fmt::format(
	    R"({{"query":"query($m:Int,$u:Int){{MediaList(mediaId:$m,userId:$u){{id}}}}",)"
	    R"("variables":{{"m":{},"u":{}}}}})",
	    mediaId, userId.empty() ? "0" : userId);
}

std::string listRequest(mediaType type, const std::string& user) {
	return fmt::format(
	    R"({{"query":"query($u:String,$t:MediaType){{MediaListCollection(userName:$u,type:$t))"
	    R"({{lists{{entries{{status score(format:POINT_10) progress progressVolumes repeat )"
	    R"(startedAt{{year month day}} completedAt{{year month day}} )"
	    R"(media{{id idMal title{{romaji}} episodes chapters volumes status )"
	    R"(nextAiringEpisode{{episode}}}}}}}}}}}}",)"
	    R"("variables":{{"u":{},"t":"{}"}}}})",
	    jsonString(user), typeName(type));
}

result<std::vector<entry>> parseList(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& lists =
	    reply.ok() ? reply.value()["data"]["MediaListCollection"]["lists"] : json::value();
	if (not lists.isArray())
		return result<std::vector<entry>>::failure("unexpected reply from AniList");
	auto dayOf = [](const json::value& fuzzy) {
		return isoDate::fromParts(countOrZero(fuzzy["year"]), countOrZero(fuzzy["month"]),
		                          countOrZero(fuzzy["day"]));
	};
	std::vector<entry> found;
	for (const auto& list : lists.items()) {
		for (const auto& item : list["entries"].items()) {
			auto status = statusOf(item["status"].asString().value_or(""));
			auto media = detailsOf(item["media"], type);
			if (not status or media.facts.anilistId == 0 or media.name.empty())
				continue;
			entry e;
			e.type = type;
			e.title = media.name;
			e.status = *status;
			e.score = static_cast<int>(item["score"].asNumber().value_or(0.0) + 0.5);
			e.progress = countOrZero(item["progress"]);
			e.volumes = countOrZero(item["progressVolumes"]);
			e.rewatched = countOrZero(item["repeat"]);
			e.startDate = dayOf(item["startedAt"]);
			e.finishDate = dayOf(item["completedAt"]);
			e.anilistId = media.facts.anilistId;
			e.malId = media.facts.malId;
			e.totalUnits = media.facts.totalUnits;
			e.totalVolumes = media.facts.totalVolumes;
			e.released = media.facts.released;
			found.push_back(std::move(e));
		}
	}
	return result<std::vector<entry>>::success(std::move(found));
}

std::string batchRequest(mediaType type, const std::vector<int>& ids, bool byMal) {
	std::string list;
	for (int id : ids)
		list += (list.empty() ? "" : ",") + std::to_string(id);
	return fmt::format(
	    R"({{"query":"query($ids:[Int],$type:MediaType){{Page(perPage:{}){{)"
	    R"(media({}:$ids,type:$type){{id idMal title{{romaji}} episodes chapters volumes )"
	    R"(status nextAiringEpisode{{episode}}}}}}}}",)"
	    R"("variables":{{"ids":[{}],"type":"{}"}}}})",
	    kBatchSize, byMal ? "idMal_in" : "id_in", list, typeName(type));
}

result<std::vector<web::details>> parseBatch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& media = reply.ok() ? reply.value()["data"]["Page"]["media"] : json::value();
	if (not media.isArray())
		return result<std::vector<web::details>>::failure("unexpected reply from AniList");
	std::vector<web::details> found;
	for (const auto& item : media.items())
		found.push_back(detailsOf(item, type));
	return result<std::vector<web::details>>::success(std::move(found));
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

std::optional<std::string> tracker::ask(const std::string& query, const std::string& token) const {
	std::vector<std::string> headers = {"Content-Type: application/json",
	                                    "Accept: application/json"};
	if (not token.empty())
		headers.push_back(web::bearer(token));
	return net_.fetch({"POST", kApiUrl, headers, query});
}

result<web::loginStart> tracker::startLogin() {
	if (net_.ids.anilist.empty())
		return result<web::loginStart>::failure(
		    "this Catalist has no AniList client id: register it at "
		    "https://anilist.co/settings/developer (redirect URL "
		    "https://anilist.co/api/v2/oauth/pin) and set CATALIST_ANILIST_CLIENT_ID");
	return result<web::loginStart>::success(
	    {fmt::format("https://anilist.co/api/v2/oauth/authorize?client_id={}&response_type=token",
	                 net_.ids.anilist),
	     "Open this page, let Catalist in, and copy the token AniList shows.",
	     {{"Token", true}}});
}

result<account> tracker::finishLogin(const std::vector<std::string>& answers) {
	std::string token = answers.empty() ? "" : answers[0];
	if (token.empty())
		return result<account>::failure("no token given");
	auto body = ask(R"({"query":"query{Viewer{id name}}"})", token);
	auto reply = body ? json::parse(*body) : result<json::value>::failure("");
	const json::value& viewer = reply.ok() ? reply.value()["data"]["Viewer"] : json::value();
	auto name = viewer["name"].asString();
	if (not name)
		return result<account>::failure("AniList did not take the token, or could not be reached");
	constexpr long long kYear = 365LL * 24 * 60 * 60;
	return result<account>::success({provider::kAniList, *name,
	                                 std::to_string(countOrZero(viewer["id"])), token, "",
	                                 net_.now() + kYear});
}

result<std::vector<entry>> tracker::pull(mediaType type, const std::string& user) {
	auto body = ask(listRequest(type, user));
	// AniList answers 404 for a user it has not, and for a private list.
	if (not body)
		return result<std::vector<entry>>::failure(
		    fmt::format("could not reach AniList, or it has no public list of {}", user));
	return parseList(*body, type);
}

result<bool> tracker::push(const entry& title, const account& login) {
	if (title.anilistId <= 0)
		return result<bool>::failure(web::notLinked(provider::kAniList));
	auto body = ask(saveRequest(title), login.token);
	if (not body)
		return result<bool>::failure("could not reach AniList, or it did not take the change");
	auto reply = json::parse(*body);
	if (not reply.ok() or reply.value()["data"]["SaveMediaListEntry"]["id"].isNull())
		return result<bool>::failure("AniList did not take the change");
	return result<bool>::success(true);
}

result<bool> tracker::remove(mediaType, int siteId, const account& login) {
	// AniList answers an error, which curl -f makes nothing, for a title that
	// is not on the list: it is gone already.
	auto body = ask(listEntryRequest(siteId, login.userId), login.token);
	auto reply = body ? json::parse(*body) : result<json::value>::failure("");
	int listId = reply.ok() ? countOrZero(reply.value()["data"]["MediaList"]["id"]) : 0;
	if (listId == 0)
		return result<bool>::success(true);
	pause();
	auto gone = ask(fmt::format(R"({{"query":"mutation($i:Int){{DeleteMediaListEntry(id:$i))"
	                            R"({{deleted}}}}","variables":{{"i":{}}}}})",
	                            listId),
	                login.token);
	if (not gone)
		return result<bool>::failure("could not reach AniList, or it did not take the change");
	return result<bool>::success(true);
}

result<account> tracker::current(const account& login) {
	if (web::runningOut(login, net_.now()))
		return result<account>::failure(
		    "your AniList login has run out: log in again with 'catalist login anilist'");
	return result<account>::success(login);
}

result<web::details> tracker::lookUp(const entry& known) {
	if (known.anilistId <= 0 and known.malId <= 0)
		return result<web::details>::failure("the title has no id to look up");
	auto body = ask(lookupRequest({known.type, known.anilistId, known.malId}));
	if (not body)
		return result<web::details>::failure("could not reach AniList");
	return parseLookup(*body, known.type);
}

std::vector<result<web::details>> tracker::lookUpMany(const std::vector<entry>& known) {
	using outcome = result<web::details>;
	std::vector<outcome> found(known.size(), outcome::failure("AniList does not know the title"));
	bool asked = false;
	bool reached = true;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		for (bool byMal : {false, true}) {
			// The titles this kind of request finds: by AniList's id when
			// they have one, else by MyAnimeList's.
			std::vector<size_t> wanted;
			for (size_t i = 0; i < known.size(); ++i) {
				const entry& each = known[i];
				bool fits = byMal ? each.anilistId <= 0 and each.malId > 0 : each.anilistId > 0;
				if (each.type == type and fits)
					wanted.push_back(i);
				if (each.anilistId <= 0 and each.malId <= 0)
					found[i] = outcome::failure("the title has no id to look up");
			}
			for (size_t from = 0; from < wanted.size(); from += kBatchSize) {
				std::vector<size_t> batch(wanted.begin() + static_cast<std::ptrdiff_t>(from),
				                          wanted.begin() + static_cast<std::ptrdiff_t>(std::min(
				                                               from + kBatchSize, wanted.size())));
				std::vector<int> ids;
				for (size_t i : batch)
					ids.push_back(byMal ? known[i].malId : known[i].anilistId);
				if (asked and reached)
					pause();
				asked = true;
				auto body = reached ? ask(batchRequest(type, ids, byMal)) : std::nullopt;
				auto titles =
				    body ? parseBatch(*body, type)
				         : result<std::vector<web::details>>::failure("could not reach AniList");
				reached = reached and body.has_value();
				for (size_t i : batch) {
					if (not titles.ok()) {
						found[i] = outcome::failure(titles.errorMessage());
						continue;
					}
					for (const auto& title : titles.value()) {
						int id = byMal ? title.facts.malId : title.facts.anilistId;
						if (id == (byMal ? known[i].malId : known[i].anilistId))
							found[i] = outcome::success(title);
					}
				}
			}
		}
	}
	return found;
}

result<std::vector<web::match>> tracker::search(mediaType type, const std::string& query) {
	auto body = ask(searchRequest(type, query));
	if (not body)
		return result<std::vector<web::match>>::failure("could not reach AniList");
	return parseSearch(*body, type);
}

}  // namespace catalist::anilist
