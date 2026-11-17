#include "web/kitsu.hpp"

#include <cstdlib>
#include <format>

#include "web/json.hpp"

namespace catalist::kitsu {

namespace {

// Kitsu writes ids, and its average rating, as strings: "46474", "84.53".
int IdOf(const json::Value& field) {
	auto text = field.AsString();
	int id = text ? std::atoi(text->c_str()) : 0;
	return id > 0 ? id : 0;
}

// The MyAnimeList and AniList ids among the mappings a title names, found in
// the reply's `included`.
void ReadMappings(const json::Value& item, const json::Value& included, MediaType type, int& mal_id,
                  int& anilist_id) {
	std::string kind = TypeKey(type);
	for (const auto& wanted : item["relationships"]["mappings"]["data"].Items()) {
		for (const auto& mapping : included.Items()) {
			bool named = mapping["type"].AsString().value_or("") == "mappings" and
			             mapping["id"].AsString() == wanted["id"].AsString();
			if (not named)
				continue;
			std::string site = mapping["attributes"]["externalSite"].AsString().value_or("");
			int id = IdOf(mapping["attributes"]["externalId"]);
			if (site == "myanimelist/" + kind)
				mal_id = id;
			else if (site == "anilist/" + kind)
				anilist_id = id;
		}
	}
}

std::optional<WatchStatus> StatusOf(const std::string& library) {
	if (library == "current")
		return WatchStatus::kWatching;
	if (library == "completed")
		return WatchStatus::kCompleted;
	if (library == "on_hold")
		return WatchStatus::kOnHold;
	if (library == "dropped")
		return WatchStatus::kDropped;
	if (library == "planned")
		return WatchStatus::kPlanToWatch;
	return std::nullopt;
}

std::string LibraryStatus(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return "current";
		case WatchStatus::kCompleted:
			return "completed";
		case WatchStatus::kOnHold:
			return "on_hold";
		case WatchStatus::kDropped:
			return "dropped";
		case WatchStatus::kPlanToWatch:
			return "planned";
		// With reconsuming (see LibraryAttributes).
		case WatchStatus::kRewatching:
			return "current";
	}
	return "planned";
}

// The account's library entry of a title, if it has one.
std::string LibraryEntryUrl(MediaType type, int kitsu_id, const Account& login) {
	return std::format("{}/library-entries?filter[userId]={}&filter[{}Id]={}&page[limit]=1",
	                   kApiUrl, login.user_id, TypeKey(type), kitsu_id);
}

// An ISO 8601 time at midnight, UTC, or null.
std::string TimeOf(const std::optional<IsoDate>& day) {
	return day ? std::format(R"("{}T00:00:00.000Z")", day->ToString()) : "null";
}

int UnitsOf(const json::Value& attributes, MediaType type) {
	return attributes[type == MediaType::kAnime ? "episodeCount" : "chapterCount"].AsCount();
}

web::Match MatchOf(const json::Value& item, const json::Value& included, MediaType type) {
	const json::Value& attributes = item["attributes"];
	web::Match found;
	found.kitsu_id = IdOf(item["id"]);
	found.name = attributes["canonicalTitle"].AsString().value_or("");
	found.total_units = UnitsOf(attributes, type);
	found.total_volumes = attributes["volumeCount"].AsCount();
	found.year = attributes["startDate"].AsString().value_or("").substr(0, 4);
	auto rating = attributes["averageRating"].AsString();
	found.score = rating ? std::atof(rating->c_str()) / 10.0 : 0.0;
	ReadMappings(item, included, type, found.mal_id, found.anilist_id);
	return found;
}

}  // namespace

std::string SearchUrl(MediaType type, const std::string& query) {
	return std::format("{}/{}?filter[text]={}&page[limit]=10&include=mappings", kApiUrl,
	                   TypeKey(type), web::UrlEncoded(query));
}

web::Answer<std::vector<web::Match>> ParseSearch(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	const json::Value& data = reply.has_value() ? reply.value()["data"] : json::Value();
	if (not data.IsArray())
		return web::Refused("unexpected reply from Kitsu");
	std::vector<web::Match> found;
	for (const auto& item : data.Items()) {
		auto match = MatchOf(item, reply.value()["included"], type);
		if (match.kitsu_id > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return found;
}

std::string TitleUrl(MediaType type, int kitsu_id) {
	return std::format("{}/{}/{}?include=mappings", kApiUrl, TypeKey(type), kitsu_id);
}

// Kitsu's statuses: current, finished, tba, unreleased and upcoming.
web::Answer<web::Details> ParseTitle(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	if (not reply.has_value() or reply.value()["data"]["id"].IsNull())
		return web::Refused("Kitsu does not know the title");
	const json::Value& item = reply.value()["data"];
	auto match = MatchOf(item, reply.value()["included"], type);
	std::string status = item["attributes"]["status"].AsString().value_or("");
	web::Details found;
	found.name = match.name;
	found.facts = {match.total_units, match.total_volumes, 0,
	               match.anilist_id,  match.mal_id,        match.kitsu_id};
	found.ongoing = status == "current";
	if (status == "finished")
		found.facts.released = match.total_units;
	return found;
}

std::string MappingUrl(MediaType type, int mal_id) {
	return std::format(
	    "{}/mappings?filter[externalSite]=myanimelist/{}&filter[externalId]={}&include=item",
	    kApiUrl, TypeKey(type), mal_id);
}

int ParseMapping(const std::string& body) {
	auto reply = json::Parse(body);
	if (not reply.has_value())
		return 0;
	for (const auto& mapping : reply.value()["data"].Items())
		return IdOf(mapping["relationships"]["item"]["data"]["id"]);
	return 0;
}

std::string LibraryUrl(MediaType type, const std::string& user_id) {
	std::string kind = TypeKey(type);
	return std::format(
	    "{}/library-entries?filter[userId]={}&filter[kind]={}&include={},{}.mappings"
	    "&page[limit]=500",
	    kApiUrl, user_id, kind, kind, kind);
}

web::Answer<std::vector<Entry>> ParseLibrary(const std::string& body, MediaType type,
                                             std::string& next) {
	auto reply = json::Parse(body);
	const json::Value& data = reply.has_value() ? reply.value()["data"] : json::Value();
	if (not data.IsArray())
		return web::Refused("unexpected reply from Kitsu");
	const json::Value& included = reply.value()["included"];
	std::string kind = TypeKey(type);
	std::vector<Entry> found;
	for (const auto& item : data.Items()) {
		const json::Value& attributes = item["attributes"];
		bool again = attributes["reconsuming"].AsBool().value_or(false);
		auto status = again ? std::optional(WatchStatus::kRewatching)
		                    : StatusOf(attributes["status"].AsString().value_or(""));
		auto media_id = item["relationships"][kind]["data"]["id"].AsString();
		const json::Value* media = nullptr;
		for (const auto& each : included.Items())
			if (each["type"].AsString().value_or("") == kind and each["id"].AsString() == media_id)
				media = &each;
		if (not status or not media)
			continue;
		auto match = MatchOf(*media, included, type);
		Entry e;
		e.type = type;
		e.title = match.name;
		e.kitsu_id = match.kitsu_id;
		e.mal_id = match.mal_id;
		e.anilist_id = match.anilist_id;
		e.total_units = match.total_units;
		e.total_volumes = match.total_volumes;
		if ((*media)["attributes"]["status"].AsString().value_or("") == "finished")
			e.released = match.total_units;
		e.status = *status;
		e.progress = attributes["progress"].AsCount();
		e.rewatched = attributes["reconsumeCount"].AsCount();
		// ratingTwenty runs from 2 to 20.
		e.score = (attributes["ratingTwenty"].AsCount() + 1) / 2;
		e.start_date = web::DayOf(attributes["startedAt"].AsString().value_or(""));
		e.finish_date = web::DayOf(attributes["finishedAt"].AsString().value_or(""));
		if (e.kitsu_id > 0 and not e.title.empty())
			found.push_back(std::move(e));
	}
	next = reply.value()["links"]["next"].AsString().value_or("");
	return found;
}

web::Answer<std::optional<web::SiteEntry>> ParseEntry(const std::string& body) {
	auto reply = json::Parse(body);
	const json::Value& data = reply.has_value() ? reply.value()["data"] : json::Value();
	if (not data.IsArray())
		return web::Refused("unexpected reply from Kitsu");
	if (data.Items().empty())
		return std::optional<web::SiteEntry>();
	const json::Value& attributes = data.Items().front()["attributes"];
	bool again = attributes["reconsuming"].AsBool().value_or(false);
	web::SiteEntry found;
	found.status = again ? WatchStatus::kRewatching
	                     : StatusOf(attributes["status"].AsString().value_or(""))
	                           .value_or(WatchStatus::kPlanToWatch);
	// ratingTwenty runs from 2 to 20.
	found.score = attributes["ratingTwenty"].AsCount();
	found.progress = attributes["progress"].AsCount();
	found.rewatched = attributes["reconsumeCount"].AsCount();
	found.start_date = web::DayOf(attributes["startedAt"].AsString().value_or(""));
	found.finish_date = web::DayOf(attributes["finishedAt"].AsString().value_or(""));
	found.hidden = attributes["private"].AsBool().value_or(false);
	return found;
}

std::string LibraryAttributes(const Entry& title, const web::Sending& what) {
	std::string attributes;
	auto add = [&attributes, &what](unsigned field, const std::string& pair) {
		if (what.fields & field)
			attributes += (attributes.empty() ? "" : ",") + pair;
	};
	bool again = title.status == WatchStatus::kRewatching;
	add(site_field::kStatus, std::format(R"("status":"{}")", LibraryStatus(title.status)));
	add(site_field::kStatus, std::format(R"("reconsuming":{})", again ? "true" : "false"));
	add(site_field::kProgress, std::format(R"("progress":{})", title.progress));
	add(site_field::kRewatched, std::format(R"("reconsumeCount":{})", title.rewatched));
	// ratingTwenty runs from 2 to 20; null is no rating.
	add(site_field::kScore,
	    std::format(R"("ratingTwenty":{})",
	                title.score > 0 ? std::to_string(title.score * 2) : "null"));
	add(site_field::kStarted, std::format(R"("startedAt":{})", TimeOf(title.start_date)));
	add(site_field::kFinished, std::format(R"("finishedAt":{})", TimeOf(title.finish_date)));
	add(site_field::kHidden, std::format(R"("private":{})", what.hidden ? "true" : "false"));
	return "{" + attributes + "}";
}

std::vector<std::string> Tracker::AsUser(const Account& login) {
	return {"Content-Type: application/vnd.api+json", "Accept: application/vnd.api+json",
	        web::Bearer(login.token)};
}

web::Answer<int> Tracker::LibraryEntry(MediaType type, int kitsu_id, const Account& login) const {
	auto body = Send({"GET", LibraryEntryUrl(type, kitsu_id, login), AsUser(login), ""});
	if (not body)
		return std::unexpected(body.error());
	auto reply = json::Parse(*body);
	if (not reply.has_value())
		return web::Refused("unexpected reply from Kitsu");
	for (const auto& found : reply.value()["data"].Items())
		return IdOf(found["id"]);
	return 0;
}

web::Answer<std::vector<Entry>> Tracker::Pull(MediaType type, const std::string& user) {
	auto people = Get(std::format("{}/users?filter[name]={}", kApiUrl, web::UrlEncoded(user)));
	if (not people)
		return std::unexpected(people.error());
	auto reply = json::Parse(*people);
	std::string user_id;
	if (reply.has_value() and not reply.value()["data"].Items().empty())
		user_id = reply.value()["data"].Items().front()["id"].AsString().value_or("");
	if (user_id.empty())
		return web::Refused(std::format("Kitsu has no user named {}", user));
	std::vector<Entry> all;
	for (std::string page = LibraryUrl(type, user_id); not page.empty();) {
		Pause();
		auto body = Get(page);
		if (not body)
			return std::unexpected(body.error());
		auto entries = ParseLibrary(*body, type, page);
		if (not entries.has_value())
			return entries;
		all.insert(all.end(), entries.value().begin(), entries.value().end());
	}
	return all;
}

web::Answer<std::optional<web::SiteEntry>> Tracker::EntryOf(const Entry& title,
                                                            const Account& login) {
	if (title.kitsu_id <= 0)
		return web::NotLinked(Provider::kKitsu);
	auto body =
	    Send({"GET", LibraryEntryUrl(title.type, title.kitsu_id, login), AsUser(login), ""});
	if (not body)
		return std::unexpected(body.error());
	return ParseEntry(*body);
}

web::Answer<void> Tracker::Push(const Entry& title, const Account& login,
                                const web::Sending& what) {
	if (title.kitsu_id <= 0)
		return web::NotLinked(Provider::kKitsu);
	auto existing = LibraryEntry(title.type, title.kitsu_id, login);
	if (not existing)
		return std::unexpected(existing.error());
	Pause();
	std::string kind = TypeKey(title.type);
	auto body =
	    *existing > 0
	        ? Send({"PATCH", std::format("{}/library-entries/{}", kApiUrl, *existing),
	                AsUser(login),
	                std::format(R"({{"data":{{"id":"{}","type":"libraryEntries",)"
	                            R"("attributes":{}}}}})",
	                            *existing, LibraryAttributes(title, what))})
	        : Send({"POST", kApiUrl + "/library-entries", AsUser(login),
	                std::format(
	                    R"({{"data":{{"type":"libraryEntries","attributes":{},)"
	                    R"("relationships":{{"user":{{"data":{{"type":"users",)"
	                    R"("id":"{}"}}}},"{}":{{"data":{{"type":"{}","id":"{}"}}}}}}}}}})",
	                    LibraryAttributes(title, {site_field::kAll | what.fields, what.hidden}),
	                    login.user_id, kind, kind, title.kitsu_id)});
	if (not body)
		return std::unexpected(body.error());
	return {};
}

web::Answer<void> Tracker::Remove(MediaType type, int site_id, const Account& login) {
	auto existing = LibraryEntry(type, site_id, login);
	if (not existing)
		return std::unexpected(existing.error());
	if (*existing == 0)
		return {};
	Pause();
	auto body = Send(
	    {"DELETE", std::format("{}/library-entries/{}", kApiUrl, *existing), AsUser(login), ""});
	// 404: gone already.
	if (not body and body.error().status != 404)
		return std::unexpected(body.error());
	return {};
}

web::Answer<std::string> Tracker::Send(const web::Request& asked) const {
	return web::Checked(net_.fetch(asked), Provider::kKitsu);
}

web::Answer<std::string> Tracker::Get(const std::string& url) const {
	return Send({"GET", url, {"Accept: application/vnd.api+json"}, ""});
}

web::Answer<std::vector<web::Match>> Tracker::Search(MediaType type, const std::string& query) {
	auto body = Get(SearchUrl(type, query));
	if (not body)
		return std::unexpected(body.error());
	return ParseSearch(*body, type);
}

web::Answer<web::Details> Tracker::LookUp(const Entry& known) {
	int id = known.kitsu_id;
	if (id <= 0 and known.mal_id > 0) {
		auto mapped = Get(MappingUrl(known.type, known.mal_id));
		if (not mapped)
			return std::unexpected(mapped.error());
		id = ParseMapping(*mapped);
		if (id <= 0)
			return web::Refused("Kitsu does not know the title");
		net_.pause();
	}
	if (id <= 0)
		return web::Refused("the title has no id to look up");
	auto body = Get(TitleUrl(known.type, id));
	if (not body and body.error().status == 404)
		return web::Refused("Kitsu does not know the title");
	if (not body)
		return std::unexpected(body.error());
	return ParseTitle(*body, known.type);
}

web::Answer<web::LoginStart> Tracker::StartLogin() {
	return web::LoginStart{
	    "",
	    "Kitsu takes your email and password once, for a token; the password is\n"
	    "not kept.",
	    {{"Email", false}, {"Password", true}}};
}

web::Answer<Account> Tracker::AccountFor(std::vector<std::pair<std::string, std::string>> fields) {
	if (not net_.ids.kitsu.empty()) {
		fields.emplace_back("client_id", net_.ids.kitsu);
		fields.emplace_back("client_secret", net_.ids.kitsu_secret);
	}
	auto reply = Send({"POST",
	                   kTokenUrl,
	                   {"Content-Type: application/x-www-form-urlencoded"},
	                   web::FormEncoded(fields)});
	if (not reply and reply.error().why == web::Failure::Kind::kUnreachable)
		return std::unexpected(reply.error());
	auto token = reply ? web::ParseTokenReply(*reply) : std::nullopt;
	if (not token)
		return web::Refused("Kitsu did not take the login (is the email right?)");
	auto body = Send({"GET",
	                  kApiUrl + "/users?filter[self]=true",
	                  {"Accept: application/vnd.api+json", web::Bearer(token->token)},
	                  ""});
	auto me = body ? json::Parse(*body) : std::unexpected("");
	const json::Value* user = nullptr;
	if (me.has_value() and not me.value()["data"].Items().empty())
		user = &me.value()["data"].Items().front();
	auto name = user ? (*user)["attributes"]["name"].AsString() : std::nullopt;
	if (not name)
		return web::Refused("Kitsu did not say whose login it is");
	return Account{Provider::kKitsu, *name,          (*user)["id"].AsString().value_or(""),
	               token->token,     token->renewal, net_.now() + token->lifetime};
}

web::Answer<Account> Tracker::FinishLogin(const std::vector<std::string>& answers) {
	if (answers.size() < 2 or answers[0].empty() or answers[1].empty())
		return web::Refused("give the email and the password");
	return AccountFor(
	    {{"grant_type", "password"}, {"username", answers[0]}, {"password", answers[1]}});
}

web::Answer<Account> Tracker::Current(const Account& login) {
	if (not web::RunningOut(login, net_.now()))
		return login;
	auto renewed = AccountFor({{"grant_type", "refresh_token"}, {"refresh_token", login.renewal}});
	if (not renewed and renewed.error().why == web::Failure::Kind::kUnreachable)
		return renewed;
	if (not renewed)
		return std::unexpected(
		    web::Failure{web::Failure::Kind::kLoginRefused,
		                 "your Kitsu login has run out: log in again with 'catalist login kitsu'"});
	return renewed;
}

}  // namespace catalist::kitsu
