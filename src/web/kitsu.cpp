#include "web/kitsu.hpp"

#include <cstdlib>
#include <format>

#include "web/json.hpp"

namespace catalist::kitsu {

namespace {

// Kitsu writes ids, and its average rating, as strings: "46474", "84.53".
int idOf(const json::value& field) {
	auto text = field.asString();
	int id = text ? std::atoi(text->c_str()) : 0;
	return id > 0 ? id : 0;
}

// The MyAnimeList and AniList ids among the mappings a title names, found in
// the reply's `included`.
void readMappings(const json::value& item, const json::value& included, mediaType type, int& malId,
                  int& anilistId) {
	std::string kind = typeKey(type);
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

std::optional<watchStatus> statusOf(const std::string& library) {
	if (library == "current")
		return watchStatus::kWatching;
	if (library == "completed")
		return watchStatus::kCompleted;
	if (library == "on_hold")
		return watchStatus::kOnHold;
	if (library == "dropped")
		return watchStatus::kDropped;
	if (library == "planned")
		return watchStatus::kPlanToWatch;
	return std::nullopt;
}

std::string libraryStatus(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return "current";
		case watchStatus::kCompleted:
			return "completed";
		case watchStatus::kOnHold:
			return "on_hold";
		case watchStatus::kDropped:
			return "dropped";
		case watchStatus::kPlanToWatch:
			return "planned";
		// With reconsuming (see libraryAttributes).
		case watchStatus::kRewatching:
			return "current";
	}
	return "planned";
}

// The account's library entry of a title, if it has one.
std::string libraryEntryUrl(mediaType type, int kitsuId, const account& login) {
	return std::format("{}/library-entries?filter[userId]={}&filter[{}Id]={}&page[limit]=1",
	                   kApiUrl, login.userId, typeKey(type), kitsuId);
}

// An ISO 8601 time at midnight, UTC, or null.
std::string timeOf(const std::optional<isoDate>& day) {
	return day ? std::format(R"("{}T00:00:00.000Z")", day->toString()) : "null";
}

int unitsOf(const json::value& attributes, mediaType type) {
	return attributes[type == mediaType::kAnime ? "episodeCount" : "chapterCount"].asCount();
}

web::match matchOf(const json::value& item, const json::value& included, mediaType type) {
	const json::value& attributes = item["attributes"];
	web::match found;
	found.kitsuId = idOf(item["id"]);
	found.name = attributes["canonicalTitle"].asString().value_or("");
	found.totalUnits = unitsOf(attributes, type);
	found.totalVolumes = attributes["volumeCount"].asCount();
	found.year = attributes["startDate"].asString().value_or("").substr(0, 4);
	auto rating = attributes["averageRating"].asString();
	found.score = rating ? std::atof(rating->c_str()) / 10.0 : 0.0;
	readMappings(item, included, type, found.malId, found.anilistId);
	return found;
}

}  // namespace

std::string searchUrl(mediaType type, const std::string& query) {
	return std::format("{}/{}?filter[text]={}&page[limit]=10&include=mappings", kApiUrl,
	                   typeKey(type), web::urlEncoded(query));
}

web::answer<std::vector<web::match>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& data = reply.has_value() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return web::refused("unexpected reply from Kitsu");
	std::vector<web::match> found;
	for (const auto& item : data.items()) {
		auto match = matchOf(item, reply.value()["included"], type);
		if (match.kitsuId > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return found;
}

std::string titleUrl(mediaType type, int kitsuId) {
	return std::format("{}/{}/{}?include=mappings", kApiUrl, typeKey(type), kitsuId);
}

// Kitsu's statuses: current, finished, tba, unreleased and upcoming.
web::answer<web::details> parseTitle(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.has_value() or reply.value()["data"]["id"].isNull())
		return web::refused("Kitsu does not know the title");
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
	return found;
}

std::string mappingUrl(mediaType type, int malId) {
	return std::format(
	    "{}/mappings?filter[externalSite]=myanimelist/{}&filter[externalId]={}&include=item",
	    kApiUrl, typeKey(type), malId);
}

int parseMapping(const std::string& body) {
	auto reply = json::parse(body);
	if (not reply.has_value())
		return 0;
	for (const auto& mapping : reply.value()["data"].items())
		return idOf(mapping["relationships"]["item"]["data"]["id"]);
	return 0;
}

std::string libraryUrl(mediaType type, const std::string& userId) {
	std::string kind = typeKey(type);
	return std::format(
	    "{}/library-entries?filter[userId]={}&filter[kind]={}&include={},{}.mappings"
	    "&page[limit]=500",
	    kApiUrl, userId, kind, kind, kind);
}

web::answer<std::vector<entry>> parseLibrary(const std::string& body, mediaType type,
                                             std::string& next) {
	auto reply = json::parse(body);
	const json::value& data = reply.has_value() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return web::refused("unexpected reply from Kitsu");
	const json::value& included = reply.value()["included"];
	std::string kind = typeKey(type);
	std::vector<entry> found;
	for (const auto& item : data.items()) {
		const json::value& attributes = item["attributes"];
		bool again = attributes["reconsuming"].asBool().value_or(false);
		auto status = again ? std::optional(watchStatus::kRewatching)
		                    : statusOf(attributes["status"].asString().value_or(""));
		auto mediaId = item["relationships"][kind]["data"]["id"].asString();
		const json::value* media = nullptr;
		for (const auto& each : included.items())
			if (each["type"].asString().value_or("") == kind and each["id"].asString() == mediaId)
				media = &each;
		if (not status or not media)
			continue;
		auto match = matchOf(*media, included, type);
		entry e;
		e.type = type;
		e.title = match.name;
		e.kitsuId = match.kitsuId;
		e.malId = match.malId;
		e.anilistId = match.anilistId;
		e.totalUnits = match.totalUnits;
		e.totalVolumes = match.totalVolumes;
		if ((*media)["attributes"]["status"].asString().value_or("") == "finished")
			e.released = match.totalUnits;
		e.status = *status;
		e.progress = attributes["progress"].asCount();
		e.rewatched = attributes["reconsumeCount"].asCount();
		// ratingTwenty runs from 2 to 20.
		e.score = (attributes["ratingTwenty"].asCount() + 1) / 2;
		e.startDate = web::dayOf(attributes["startedAt"].asString().value_or(""));
		e.finishDate = web::dayOf(attributes["finishedAt"].asString().value_or(""));
		if (e.kitsuId > 0 and not e.title.empty())
			found.push_back(std::move(e));
	}
	next = reply.value()["links"]["next"].asString().value_or("");
	return found;
}

web::answer<std::optional<web::siteEntry>> parseEntry(const std::string& body) {
	auto reply = json::parse(body);
	const json::value& data = reply.has_value() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return web::refused("unexpected reply from Kitsu");
	if (data.items().empty())
		return std::optional<web::siteEntry>();
	const json::value& attributes = data.items().front()["attributes"];
	bool again = attributes["reconsuming"].asBool().value_or(false);
	web::siteEntry found;
	found.status = again ? watchStatus::kRewatching
	                     : statusOf(attributes["status"].asString().value_or(""))
	                           .value_or(watchStatus::kPlanToWatch);
	found.rawScore = attributes["ratingTwenty"].asCount();
	// ratingTwenty runs from 2 to 20.
	found.score = (attributes["ratingTwenty"].asCount() + 1) / 2;
	found.progress = attributes["progress"].asCount();
	found.rewatched = attributes["reconsumeCount"].asCount();
	found.startDate = web::dayOf(attributes["startedAt"].asString().value_or(""));
	found.finishDate = web::dayOf(attributes["finishedAt"].asString().value_or(""));
	found.hidden = attributes["private"].asBool().value_or(false);
	return found;
}

std::string libraryAttributes(const entry& title) {
	// ratingTwenty runs from 2 to 20; null is no rating.
	return std::format(R"({{"status":"{}","reconsuming":{},"progress":{},"reconsumeCount":{},)"
	                   R"("ratingTwenty":{},"startedAt":{},"finishedAt":{}}})",
	                   libraryStatus(title.status),
	                   title.status == watchStatus::kRewatching ? "true" : "false", title.progress,
	                   title.rewatched,
	                   title.score > 0 ? std::to_string(title.score * 2) : "null",
	                   timeOf(title.startDate), timeOf(title.finishDate));
}

std::vector<std::string> tracker::asUser(const account& login) {
	return {"Content-Type: application/vnd.api+json", "Accept: application/vnd.api+json",
	        web::bearer(login.token)};
}

web::answer<int> tracker::libraryEntry(mediaType type, int kitsuId, const account& login) const {
	auto body = send({"GET", libraryEntryUrl(type, kitsuId, login), asUser(login), ""});
	if (not body)
		return std::unexpected(body.error());
	auto reply = json::parse(*body);
	if (not reply.has_value())
		return web::refused("unexpected reply from Kitsu");
	for (const auto& found : reply.value()["data"].items())
		return idOf(found["id"]);
	return 0;
}

web::answer<std::vector<entry>> tracker::pull(mediaType type, const std::string& user) {
	auto people = get(std::format("{}/users?filter[name]={}", kApiUrl, web::urlEncoded(user)));
	if (not people)
		return std::unexpected(people.error());
	auto reply = json::parse(*people);
	std::string userId;
	if (reply.has_value() and not reply.value()["data"].items().empty())
		userId = reply.value()["data"].items().front()["id"].asString().value_or("");
	if (userId.empty())
		return web::refused(std::format("Kitsu has no user named {}", user));
	std::vector<entry> all;
	for (std::string page = libraryUrl(type, userId); not page.empty();) {
		pause();
		auto body = get(page);
		if (not body)
			return std::unexpected(body.error());
		auto entries = parseLibrary(*body, type, page);
		if (not entries.has_value())
			return entries;
		all.insert(all.end(), entries.value().begin(), entries.value().end());
	}
	return all;
}

web::answer<std::optional<web::siteEntry>> tracker::entryOf(const entry& title,
                                                            const account& login) {
	if (title.kitsuId <= 0)
		return web::notLinked(provider::kKitsu);
	auto body =
	    send({"GET", libraryEntryUrl(title.type, title.kitsuId, login), asUser(login), ""});
	if (not body)
		return std::unexpected(body.error());
	return parseEntry(*body);
}

web::answer<void> tracker::push(const entry& title, const account& login) {
	if (title.kitsuId <= 0)
		return web::notLinked(provider::kKitsu);
	auto existing = libraryEntry(title.type, title.kitsuId, login);
	if (not existing)
		return std::unexpected(existing.error());
	pause();
	std::string kind = typeKey(title.type);
	auto body =
	    *existing > 0
	        ? send({"PATCH", std::format("{}/library-entries/{}", kApiUrl, *existing),
	                      asUser(login),
	                      std::format(R"({{"data":{{"id":"{}","type":"libraryEntries",)"
	                                  R"("attributes":{}}}}})",
	                                  *existing, libraryAttributes(title))})
	        : send(
	              {"POST", kApiUrl + "/library-entries", asUser(login),
	               std::format(R"({{"data":{{"type":"libraryEntries","attributes":{},)"
	                           R"("relationships":{{"user":{{"data":{{"type":"users",)"
	                           R"("id":"{}"}}}},"{}":{{"data":{{"type":"{}","id":"{}"}}}}}}}}}})",
	                           libraryAttributes(title), login.userId, kind, kind, title.kitsuId)});
	if (not body)
		return std::unexpected(body.error());
	return {};
}

web::answer<void> tracker::remove(mediaType type, int siteId, const account& login) {
	auto existing = libraryEntry(type, siteId, login);
	if (not existing)
		return std::unexpected(existing.error());
	if (*existing == 0)
		return {};
	pause();
	auto body =
	    send({"DELETE", std::format("{}/library-entries/{}", kApiUrl, *existing), asUser(login), ""});
	// 404: gone already.
	if (not body and body.error().status != 404)
		return std::unexpected(body.error());
	return {};
}

web::answer<std::string> tracker::send(const web::request& asked) const {
	return web::checked(net_.fetch(asked), provider::kKitsu);
}

web::answer<std::string> tracker::get(const std::string& url) const {
	return send({"GET", url, {"Accept: application/vnd.api+json"}, ""});
}

web::answer<std::vector<web::match>> tracker::search(mediaType type, const std::string& query) {
	auto body = get(searchUrl(type, query));
	if (not body)
		return std::unexpected(body.error());
	return parseSearch(*body, type);
}

web::answer<web::details> tracker::lookUp(const entry& known) {
	int id = known.kitsuId;
	if (id <= 0 and known.malId > 0) {
		auto mapped = get(mappingUrl(known.type, known.malId));
		if (not mapped)
			return std::unexpected(mapped.error());
		id = parseMapping(*mapped);
		if (id <= 0)
			return web::refused("Kitsu does not know the title");
		net_.pause();
	}
	if (id <= 0)
		return web::refused("the title has no id to look up");
	auto body = get(titleUrl(known.type, id));
	if (not body and body.error().status == 404)
		return web::refused("Kitsu does not know the title");
	if (not body)
		return std::unexpected(body.error());
	return parseTitle(*body, known.type);
}

web::answer<web::loginStart> tracker::startLogin() {
	return web::loginStart{
	    "",
	    "Kitsu takes your email and password once, for a token; the password is\n"
	    "not kept.",
	    {{"Email", false}, {"Password", true}}};
}

web::answer<account> tracker::accountFor(std::vector<std::pair<std::string, std::string>> fields) {
	if (not net_.ids.kitsu.empty()) {
		fields.emplace_back("client_id", net_.ids.kitsu);
		fields.emplace_back("client_secret", net_.ids.kitsuSecret);
	}
	auto reply = send({"POST",
	                   kTokenUrl,
	                   {"Content-Type: application/x-www-form-urlencoded"},
	                   web::formEncoded(fields)});
	if (not reply and reply.error().why == web::failure::kind::kUnreachable)
		return std::unexpected(reply.error());
	auto token = reply ? web::parseTokenReply(*reply) : std::nullopt;
	if (not token)
		return web::refused("Kitsu did not take the login (is the email right?)");
	auto body = send({"GET",
	                  kApiUrl + "/users?filter[self]=true",
	                  {"Accept: application/vnd.api+json", web::bearer(token->token)},
	                  ""});
	auto me = body ? json::parse(*body) : std::unexpected("");
	const json::value* user = nullptr;
	if (me.has_value() and not me.value()["data"].items().empty())
		user = &me.value()["data"].items().front();
	auto name = user ? (*user)["attributes"]["name"].asString() : std::nullopt;
	if (not name)
		return web::refused("Kitsu did not say whose login it is");
	return account{provider::kKitsu,
	               *name,
	               (*user)["id"].asString().value_or(""),
	               token->token,
	               token->renewal,
	               net_.now() + token->lifetime,
	               (*user)["attributes"]["ratingSystem"].asString().value_or("advanced")};
}

web::answer<account> tracker::finishLogin(const std::vector<std::string>& answers) {
	if (answers.size() < 2 or answers[0].empty() or answers[1].empty())
		return web::refused("give the email and the password");
	return accountFor(
	    {{"grant_type", "password"}, {"username", answers[0]}, {"password", answers[1]}});
}

web::answer<account> tracker::current(const account& login) {
	if (not web::runningOut(login, net_.now()))
		return login;
	auto renewed = accountFor({{"grant_type", "refresh_token"}, {"refresh_token", login.renewal}});
	if (not renewed and renewed.error().why == web::failure::kind::kUnreachable)
		return renewed;
	if (not renewed)
		return std::unexpected(
		    web::failure{web::failure::kind::kLoginRefused,
		                 "your Kitsu login has run out: log in again with 'catalist login kitsu'"});
	return renewed;
}

}  // namespace catalist::kitsu
