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

std::string ListStatus(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return "CURRENT";
		case WatchStatus::kCompleted:
			return "COMPLETED";
		case WatchStatus::kOnHold:
			return "PAUSED";
		case WatchStatus::kDropped:
			return "DROPPED";
		case WatchStatus::kPlanToWatch:
			return "PLANNING";
		case WatchStatus::kRewatching:
			return "REPEATING";
	}
	return "PLANNING";
}

// A FuzzyDateInput; all null clears the date.
std::string FuzzyDate(const std::optional<IsoDate>& day) {
	if (not day)
		return R"({"year":null,"month":null,"day":null})";
	return std::format(R"({{"year":{},"month":{},"day":{}}})", day->Year(), day->Month(),
	                   day->Day());
}

std::optional<WatchStatus> StatusOf(const std::string& list_status) {
	if (list_status == "CURRENT")
		return WatchStatus::kWatching;
	if (list_status == "REPEATING")
		return WatchStatus::kRewatching;
	if (list_status == "COMPLETED")
		return WatchStatus::kCompleted;
	if (list_status == "PAUSED")
		return WatchStatus::kOnHold;
	if (list_status == "DROPPED")
		return WatchStatus::kDropped;
	if (list_status == "PLANNING")
		return WatchStatus::kPlanToWatch;
	return std::nullopt;
}

// AniList's statuses: FINISHED, RELEASING, NOT_YET_RELEASED, CANCELLED and
// HIATUS.
web::Details DetailsOf(const json::Value& media, MediaType type) {
	std::string status = media["status"].AsString().value_or("");
	web::Details found;
	found.name = media["title"]["romaji"].AsString().value_or("");
	found.facts.anilist_id = media["id"].AsCount();
	found.facts.mal_id = media["idMal"].AsCount();
	found.facts.total_units = media[type == MediaType::kAnime ? "episodes" : "chapters"].AsCount();
	found.facts.total_volumes = media["volumes"].AsCount();
	found.ongoing = status == "RELEASING" or status == "HIATUS";
	if (status == "FINISHED" or status == "CANCELLED")
		found.facts.released = found.facts.total_units;
	// An airing anime: every episode before the next one to air is out.
	int next_episode = media["nextAiringEpisode"]["episode"].AsCount();
	if (found.ongoing and next_episode > 1)
		found.facts.released = next_episode - 1;
	return found;
}

}  // namespace

std::string LookupRequest(const TitleRef& wanted) {
	// AniList's own id when known; a MyAnimeList id goes in idMal.
	bool own = wanted.anilist_id > 0;
	return std::format(R"({{"query":"query($id:Int,$type:MediaType){{Media({}:$id,type:$type){{)"
	                   R"(id idMal title{{romaji}} episodes chapters volumes status )"
	                   R"(nextAiringEpisode{{episode}}}}}}",)"
	                   R"("variables":{{"id":{},"type":"{}"}}}})",
	                   own ? "id" : "idMal", own ? wanted.anilist_id : wanted.mal_id,
	                   TypeName(wanted.type));
}

web::Answer<web::Details> ParseLookup(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	if (not reply.has_value() or reply.value()["data"]["Media"].IsNull())
		return web::Refused("AniList does not know the title");
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

std::string SaveRequest(const Entry& title, const web::Sending& what) {
	// One variable a field sent: its declaration, its argument and its value.
	struct Sent {
		unsigned field;
		const char* declared;
		const char* argument;
		std::string value;
	};
	// scoreRaw is out of 100 whatever scale the person chose on AniList.
	std::vector<Sent> all = {
	    {site_field::kStatus, "$s:MediaListStatus", "status:$s",
	     std::format(R"("s":"{}")", ListStatus(title.status))},
	    {site_field::kScore, "$c:Int", "scoreRaw:$c", std::format(R"("c":{})", title.score * 10)},
	    {site_field::kProgress, "$p:Int", "progress:$p", std::format(R"("p":{})", title.progress)},
	    {site_field::kVolumes, "$v:Int", "progressVolumes:$v",
	     std::format(R"("v":{})", title.volumes)},
	    {site_field::kRewatched, "$r:Int", "repeat:$r", std::format(R"("r":{})", title.rewatched)},
	    {site_field::kStarted, "$a:FuzzyDateInput", "startedAt:$a",
	     std::format(R"("a":{})", FuzzyDate(title.start_date))},
	    {site_field::kFinished, "$f:FuzzyDateInput", "completedAt:$f",
	     std::format(R"("f":{})", FuzzyDate(title.finish_date))},
	    {site_field::kHidden, "$h:Boolean", "private:$h",
	     std::format(R"("h":{})", what.hidden ? "true" : "false")},
	};
	std::string declared = "$m:Int";
	std::string arguments = "mediaId:$m";
	std::string values = std::format(R"("m":{})", title.anilist_id);
	for (const auto& each : all) {
		if (not(what.fields & each.field))
			continue;
		declared += std::format(",{}", each.declared);
		arguments += std::format(",{}", each.argument);
		values += "," + each.value;
	}
	return std::format(
	    R"({{"query":"mutation({}){{SaveMediaListEntry({}){{id}}}}","variables":{{{}}}}})",
	    declared, arguments, values);
}

std::string ListEntryRequest(int media_id, const std::string& user_id) {
	return std::format(
	    R"({{"query":"query($m:Int,$u:Int){{MediaList(mediaId:$m,userId:$u){{id}}}}",)"
	    R"("variables":{{"m":{},"u":{}}}}})",
	    media_id, user_id.empty() ? "0" : user_id);
}

std::string EntryRequest(const Entry& title, const std::string& user_id) {
	return std::format(R"({{"query":"query($m:Int,$u:Int){{Page{{mediaList(userId:$u,mediaId:$m){{)"
	                   R"(status score(format:POINT_100) progress progressVolumes repeat private )"
	                   R"(startedAt{{year month day}} completedAt{{year month day}}}}}}}}",)"
	                   R"("variables":{{"m":{},"u":{}}}}})",
	                   title.anilist_id, user_id.empty() ? "0" : user_id);
}

web::Answer<std::optional<web::SiteEntry>> ParseEntry(const std::string& body) {
	auto reply = json::Parse(body);
	const json::Value& found =
	    reply.has_value() ? reply.value()["data"]["Page"]["mediaList"] : json::Value();
	if (not found.IsArray())
		return web::Refused("unexpected reply from AniList");
	if (found.Items().empty())
		return std::optional<web::SiteEntry>();
	const json::Value& item = found.Items().front();
	auto day_of = [](const json::Value& fuzzy) {
		return IsoDate::FromParts(fuzzy["year"].AsCount(), fuzzy["month"].AsCount(),
		                          fuzzy["day"].AsCount());
	};
	web::SiteEntry mine;
	mine.status =
	    StatusOf(item["status"].AsString().value_or("")).value_or(WatchStatus::kPlanToWatch);
	// Asked as POINT_100: out of 100 whatever the person scores in.
	mine.score = static_cast<int>(item["score"].AsNumber().value_or(0.0) + 0.5);
	mine.progress = item["progress"].AsCount();
	mine.volumes = item["progressVolumes"].AsCount();
	mine.rewatched = item["repeat"].AsCount();
	mine.start_date = day_of(item["startedAt"]);
	mine.finish_date = day_of(item["completedAt"]);
	mine.hidden = item["private"].AsBool().value_or(false);
	return mine;
}

std::string ListRequest(MediaType type, const std::string& user) {
	return std::format(
	    R"({{"query":"query($u:String,$t:MediaType){{MediaListCollection(userName:$u,type:$t))"
	    R"({{lists{{entries{{status score(format:POINT_10) progress progressVolumes repeat )"
	    R"(startedAt{{year month day}} completedAt{{year month day}} )"
	    R"(media{{id idMal title{{romaji}} episodes chapters volumes status )"
	    R"(nextAiringEpisode{{episode}}}}}}}}}}}}",)"
	    R"("variables":{{"u":{},"t":"{}"}}}})",
	    JsonString(user), TypeName(type));
}

web::Answer<std::vector<Entry>> ParseList(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	const json::Value& lists =
	    reply.has_value() ? reply.value()["data"]["MediaListCollection"]["lists"] : json::Value();
	if (not lists.IsArray())
		return web::Refused("unexpected reply from AniList");
	auto day_of = [](const json::Value& fuzzy) {
		return IsoDate::FromParts(fuzzy["year"].AsCount(), fuzzy["month"].AsCount(),
		                          fuzzy["day"].AsCount());
	};
	std::vector<Entry> found;
	for (const auto& list : lists.Items()) {
		for (const auto& item : list["entries"].Items()) {
			auto status = StatusOf(item["status"].AsString().value_or(""));
			auto media = DetailsOf(item["media"], type);
			if (not status or media.facts.anilist_id == 0 or media.name.empty())
				continue;
			Entry e;
			e.type = type;
			e.title = media.name;
			e.status = *status;
			e.score = static_cast<int>(item["score"].AsNumber().value_or(0.0) + 0.5);
			e.progress = item["progress"].AsCount();
			e.volumes = item["progressVolumes"].AsCount();
			e.rewatched = item["repeat"].AsCount();
			e.start_date = day_of(item["startedAt"]);
			e.finish_date = day_of(item["completedAt"]);
			e.anilist_id = media.facts.anilist_id;
			e.mal_id = media.facts.mal_id;
			e.total_units = media.facts.total_units;
			e.total_volumes = media.facts.total_volumes;
			e.released = media.facts.released;
			found.push_back(std::move(e));
		}
	}
	return found;
}

std::string BatchRequest(MediaType type, const std::vector<int>& ids, bool by_mal) {
	std::string list;
	for (int id : ids)
		list += (list.empty() ? "" : ",") + std::to_string(id);
	return std::format(
	    R"({{"query":"query($ids:[Int],$type:MediaType){{Page(perPage:{}){{)"
	    R"(media({}:$ids,type:$type){{id idMal title{{romaji}} episodes chapters volumes )"
	    R"(status nextAiringEpisode{{episode}}}}}}}}",)"
	    R"("variables":{{"ids":[{}],"type":"{}"}}}})",
	    kBatchSize, by_mal ? "idMal_in" : "id_in", list, TypeName(type));
}

web::Answer<std::vector<web::Details>> ParseBatch(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	const json::Value& media =
	    reply.has_value() ? reply.value()["data"]["Page"]["media"] : json::Value();
	if (not media.IsArray())
		return web::Refused("unexpected reply from AniList");
	std::vector<web::Details> found;
	for (const auto& item : media.Items())
		found.push_back(DetailsOf(item, type));
	return found;
}

web::Answer<std::vector<web::Match>> ParseSearch(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	const json::Value& media =
	    reply.has_value() ? reply.value()["data"]["Page"]["media"] : json::Value();
	if (not media.IsArray())
		return web::Refused("unexpected reply from AniList");
	std::vector<web::Match> found;
	for (const auto& item : media.Items()) {
		web::Match match;
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

web::Answer<std::string> Tracker::Ask(const std::string& query, const std::string& token) const {
	std::vector<std::string> headers = {"Content-Type: application/json",
	                                    "Accept: application/json"};
	if (not token.empty())
		headers.push_back(web::Bearer(token));
	return web::Checked(net_.fetch({"POST", kApiUrl, headers, query}), Provider::kAniList);
}

web::Answer<web::LoginStart> Tracker::StartLogin() {
	if (net_.ids.anilist.empty())
		return web::Refused(
		    "this Catalist has no AniList client id: register it at "
		    "https://anilist.co/settings/developer (redirect URL "
		    "https://anilist.co/api/v2/oauth/pin) and set CATALIST_ANILIST_CLIENT_ID");
	return web::LoginStart{
	    std::format("https://anilist.co/api/v2/oauth/authorize?client_id={}&response_type=token",
	                net_.ids.anilist),
	    "Open this page, let Catalist in, and copy the token AniList shows.",
	    {{"Token", true}}};
}

web::Answer<Account> Tracker::FinishLogin(const std::vector<std::string>& answers) {
	std::string token = answers.empty() ? "" : answers[0];
	if (token.empty())
		return web::Refused("no token given");
	auto body = Ask(R"({"query":"query{Viewer{id name}}"})", token);
	if (not body and body.error().why == web::Failure::Kind::kUnreachable)
		return std::unexpected(body.error());
	auto reply = body ? json::Parse(*body) : std::unexpected("");
	const json::Value& viewer = reply.has_value() ? reply.value()["data"]["Viewer"] : json::Value();
	auto name = viewer["name"].AsString();
	if (not name)
		return web::Refused("AniList did not take the token");
	constexpr long long kYear = 365LL * 24 * 60 * 60;
	return Account{Provider::kAniList, *name, std::to_string(viewer["id"].AsCount()), token, "",
	               net_.now() + kYear};
}

web::Answer<std::vector<Entry>> Tracker::Pull(MediaType type, const std::string& user) {
	auto body = Ask(ListRequest(type, user));
	// AniList answers 404 for a user it has not, and for a private list.
	if (not body and body.error().status == 404)
		return web::Refused(std::format("AniList has no public list of {}", user));
	if (not body)
		return std::unexpected(body.error());
	return ParseList(*body, type);
}

web::Answer<std::optional<web::SiteEntry>> Tracker::EntryOf(const Entry& title,
                                                            const Account& login) {
	if (title.anilist_id <= 0)
		return web::NotLinked(Provider::kAniList);
	auto body = Ask(EntryRequest(title, login.user_id), login.token);
	if (not body)
		return std::unexpected(body.error());
	return ParseEntry(*body);
}

web::Answer<void> Tracker::Push(const Entry& title, const Account& login,
                                const web::Sending& what) {
	if (title.anilist_id <= 0)
		return web::NotLinked(Provider::kAniList);
	auto body = Ask(SaveRequest(title, what), login.token);
	if (not body)
		return std::unexpected(body.error());
	auto reply = json::Parse(*body);
	if (not reply.has_value() or reply.value()["data"]["SaveMediaListEntry"]["id"].IsNull())
		return web::Refused("AniList did not take the change");
	return {};
}

web::Answer<void> Tracker::Remove(MediaType, int site_id, const Account& login) {
	// AniList answers 404 for a title that is not on the list: it is gone
	// already.
	auto body = Ask(ListEntryRequest(site_id, login.user_id), login.token);
	if (not body and body.error().status != 404)
		return std::unexpected(body.error());
	auto reply = body ? json::Parse(*body) : std::unexpected("");
	int list_id = reply.has_value() ? reply.value()["data"]["MediaList"]["id"].AsCount() : 0;
	if (list_id == 0)
		return {};
	Pause();
	auto gone = Ask(std::format(R"({{"query":"mutation($i:Int){{DeleteMediaListEntry(id:$i))"
	                            R"({{deleted}}}}","variables":{{"i":{}}}}})",
	                            list_id),
	                login.token);
	if (not gone)
		return std::unexpected(gone.error());
	return {};
}

web::Answer<Account> Tracker::Current(const Account& login) {
	if (web::RunningOut(login, net_.now()))
		return std::unexpected(web::Failure{
		    web::Failure::Kind::kLoginRefused,
		    "your AniList login has run out: log in again with 'catalist login anilist'"});
	return login;
}

web::Answer<web::Details> Tracker::LookUp(const Entry& known) {
	if (known.anilist_id <= 0 and known.mal_id <= 0)
		return web::Refused("the title has no id to look up");
	auto body = Ask(LookupRequest({known.type, known.anilist_id, known.mal_id}));
	if (not body and body.error().status == 404)
		return web::Refused("AniList does not know the title");
	if (not body)
		return std::unexpected(body.error());
	return ParseLookup(*body, known.type);
}

std::vector<web::Answer<web::Details>> Tracker::LookUpMany(const std::vector<Entry>& known) {
	using Outcome = web::Answer<web::Details>;
	std::vector<Outcome> found(known.size(), web::Refused("AniList does not know the title"));
	bool asked = false;
	bool reached = true;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		for (bool by_mal : {false, true}) {
			// The titles this kind of request finds: by AniList's id when
			// they have one, else by MyAnimeList's.
			std::vector<size_t> wanted;
			for (size_t i = 0; i < known.size(); ++i) {
				const Entry& each = known[i];
				bool fits = by_mal ? each.anilist_id <= 0 and each.mal_id > 0 : each.anilist_id > 0;
				if (each.type == type and fits)
					wanted.push_back(i);
				if (each.anilist_id <= 0 and each.mal_id <= 0)
					found[i] = web::Refused("the title has no id to look up");
			}
			for (size_t from = 0; from < wanted.size(); from += kBatchSize) {
				std::vector<size_t> batch(wanted.begin() + static_cast<std::ptrdiff_t>(from),
				                          wanted.begin() + static_cast<std::ptrdiff_t>(std::min(
				                                               from + kBatchSize, wanted.size())));
				std::vector<int> ids;
				for (size_t i : batch)
					ids.push_back(by_mal ? known[i].mal_id : known[i].anilist_id);
				if (asked and reached)
					Pause();
				asked = true;
				auto body = reached ? Ask(BatchRequest(type, ids, by_mal))
				                    : web::Unreachable("could not reach AniList");
				auto titles =
				    body ? ParseBatch(*body, type)
				         : web::Answer<std::vector<web::Details>>(std::unexpected(body.error()));
				reached =
				    reached and (body or body.error().why != web::Failure::Kind::kUnreachable);
				for (size_t i : batch) {
					if (not titles.has_value()) {
						found[i] = std::unexpected(titles.error());
						continue;
					}
					for (const auto& title : titles.value()) {
						int id = by_mal ? title.facts.mal_id : title.facts.anilist_id;
						if (id == (by_mal ? known[i].mal_id : known[i].anilist_id))
							found[i] = title;
					}
				}
			}
		}
	}
	return found;
}

web::Answer<std::vector<web::Match>> Tracker::Search(MediaType type, const std::string& query) {
	auto body = Ask(SearchRequest(type, query));
	if (not body)
		return std::unexpected(body.error());
	return ParseSearch(*body, type);
}

}  // namespace catalist::anilist
