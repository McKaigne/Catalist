#include "web/mal.hpp"

#include <format>

#include "web/json.hpp"

namespace catalist::mal {

namespace {

std::string fieldsOf(mediaType type) {
	return type == mediaType::kAnime ? "num_episodes,start_date,mean,status"
	                                 : "num_chapters,num_volumes,start_date,mean,status";
}

std::string listStatus(watchStatus status, mediaType type) {
	bool anime = type == mediaType::kAnime;
	switch (status) {
		case watchStatus::kWatching:
			return anime ? "watching" : "reading";
		case watchStatus::kCompleted:
			return "completed";
		case watchStatus::kOnHold:
			return "on_hold";
		case watchStatus::kDropped:
			return "dropped";
		case watchStatus::kPlanToWatch:
			return anime ? "plan_to_watch" : "plan_to_read";
		// With is_rewatching or is_rereading (see listStatusForm).
		case watchStatus::kRewatching:
			return anime ? "watching" : "reading";
	}
	return anime ? "plan_to_watch" : "plan_to_read";
}

std::optional<watchStatus> statusOf(const std::string& listStatus) {
	if (listStatus == "watching" or listStatus == "reading")
		return watchStatus::kWatching;
	if (listStatus == "completed")
		return watchStatus::kCompleted;
	if (listStatus == "on_hold")
		return watchStatus::kOnHold;
	if (listStatus == "dropped")
		return watchStatus::kDropped;
	if (listStatus == "plan_to_watch" or listStatus == "plan_to_read")
		return watchStatus::kPlanToWatch;
	return std::nullopt;
}

web::match matchOf(const json::value& node, mediaType type) {
	web::match found;
	found.malId = node["id"].asCount();
	found.name = node["title"].asString().value_or("");
	found.totalUnits = node[type == mediaType::kAnime ? "num_episodes" : "num_chapters"].asCount();
	found.totalVolumes = node["num_volumes"].asCount();
	found.year = node["start_date"].asString().value_or("").substr(0, 4);
	found.score = node["mean"].asNumber().value_or(0.0);
	return found;
}

}  // namespace

std::string searchUrl(mediaType type, const std::string& query) {
	return std::format("{}/{}?q={}&limit=10&fields={}", kApiUrl, typeKey(type),
	                   web::urlEncoded(query), fieldsOf(type));
}

web::answer<std::vector<web::match>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& data = reply.has_value() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return web::refused("unexpected reply from MyAnimeList");
	std::vector<web::match> found;
	for (const auto& item : data.items()) {
		auto match = matchOf(item["node"], type);
		if (match.malId > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return found;
}

std::string titleUrl(mediaType type, int malId) {
	return std::format("{}/{}/{}?fields={}", kApiUrl, typeKey(type), malId, fieldsOf(type));
}

// The statuses: finished_airing, currently_airing and not_yet_aired for an
// anime; finished, currently_publishing, on_hiatus, discontinued and
// not_yet_published for a manga.
web::answer<web::details> parseTitle(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.has_value() or reply.value()["id"].asCount() == 0)
		return web::refused("MyAnimeList does not know the title");
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
	return found;
}

std::string listUrl(mediaType type, const std::string& user) {
	bool anime = type == mediaType::kAnime;
	return std::format(
	    "{}/users/{}/{}?fields=list_status{{{}}},{},status&limit=1000&nsfw=true", kApiUrl,
	    web::urlEncoded(user), anime ? "animelist" : "mangalist",
	    anime ? "status,score,num_episodes_watched,num_times_rewatched,is_rewatching,start_date,"
	            "finish_date"
	          : "status,score,num_chapters_read,num_volumes_read,num_times_reread,is_rereading,"
	            "start_date,finish_date",
	    anime ? "num_episodes" : "num_chapters,num_volumes");
}

web::answer<std::vector<entry>> parseList(const std::string& body, mediaType type,
                                          std::string& next) {
	auto reply = json::parse(body);
	const json::value& data = reply.has_value() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return web::refused("unexpected reply from MyAnimeList");
	bool anime = type == mediaType::kAnime;
	std::vector<entry> found;
	for (const auto& item : data.items()) {
		const json::value& mine = item["list_status"];
		// A rewatch is a rewatch whatever the status says, as Mihon reads it.
		bool again = mine[anime ? "is_rewatching" : "is_rereading"].asBool().value_or(false);
		auto status = again ? std::optional(watchStatus::kRewatching)
		                    : statusOf(mine["status"].asString().value_or(""));
		auto match = matchOf(item["node"], type);
		if (not status or match.malId == 0 or match.name.empty())
			continue;
		entry e;
		e.type = type;
		e.title = match.name;
		e.malId = match.malId;
		e.totalUnits = match.totalUnits;
		e.totalVolumes = match.totalVolumes;
		std::string airing = item["node"]["status"].asString().value_or("");
		if (airing == "finished_airing" or airing == "finished")
			e.released = match.totalUnits;
		e.status = *status;
		e.score = mine["score"].asCount();
		e.progress = mine[anime ? "num_episodes_watched" : "num_chapters_read"].asCount();
		e.volumes = mine["num_volumes_read"].asCount();
		e.rewatched = mine[anime ? "num_times_rewatched" : "num_times_reread"].asCount();
		e.startDate = web::dayOf(mine["start_date"].asString().value_or(""));
		e.finishDate = web::dayOf(mine["finish_date"].asString().value_or(""));
		found.push_back(std::move(e));
	}
	next = reply.value()["paging"]["next"].asString().value_or("");
	return found;
}

std::string entryUrl(mediaType type, int malId) {
	bool anime = type == mediaType::kAnime;
	return std::format(
	    "{}/{}/{}?fields=my_list_status{{{}}}", kApiUrl, typeKey(type), malId,
	    anime ? "status,score,num_episodes_watched,is_rewatching,num_times_rewatched,start_date,"
	            "finish_date"
	          : "status,score,num_chapters_read,num_volumes_read,is_rereading,num_times_reread,"
	            "start_date,finish_date");
}

web::answer<std::optional<web::siteEntry>> parseEntry(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.has_value() or reply.value()["id"].asCount() == 0)
		return web::refused("unexpected reply from MyAnimeList");
	const json::value& mine = reply.value()["my_list_status"];
	if (mine.isNull())
		return std::optional<web::siteEntry>();
	bool anime = type == mediaType::kAnime;
	bool again = mine[anime ? "is_rewatching" : "is_rereading"].asBool().value_or(false);
	web::siteEntry found;
	found.status = again ? watchStatus::kRewatching
	                     : statusOf(mine["status"].asString().value_or(""))
	                           .value_or(watchStatus::kPlanToWatch);
	found.score = mine["score"].asCount();
	found.progress = mine[anime ? "num_episodes_watched" : "num_chapters_read"].asCount();
	found.volumes = mine["num_volumes_read"].asCount();
	found.rewatched = mine[anime ? "num_times_rewatched" : "num_times_reread"].asCount();
	found.startDate = web::dayOf(mine["start_date"].asString().value_or(""));
	found.finishDate = web::dayOf(mine["finish_date"].asString().value_or(""));
	return found;
}

std::string listStatusForm(const entry& title) {
	bool anime = title.type == mediaType::kAnime;
	std::vector<std::pair<std::string, std::string>> fields = {
	    {"status", listStatus(title.status, title.type)}, {"score", std::to_string(title.score)}};
	std::string again = title.status == watchStatus::kRewatching ? "true" : "false";
	if (anime) {
		fields.emplace_back("is_rewatching", again);
		fields.emplace_back("num_watched_episodes", std::to_string(title.progress));
		fields.emplace_back("num_times_rewatched", std::to_string(title.rewatched));
	} else {
		fields.emplace_back("is_rereading", again);
		fields.emplace_back("num_chapters_read", std::to_string(title.progress));
		fields.emplace_back("num_volumes_read", std::to_string(title.volumes));
		fields.emplace_back("num_times_reread", std::to_string(title.rewatched));
	}
	if (title.startDate)
		fields.emplace_back("start_date", title.startDate->toString());
	if (title.finishDate)
		fields.emplace_back("finish_date", title.finishDate->toString());
	return web::formEncoded(fields);
}

web::answer<std::vector<entry>> tracker::pull(mediaType type, const std::string& user) {
	if (auto why = cannotAsk(); not why.empty())
		return web::refused(why);
	std::vector<entry> all;
	for (std::string page = listUrl(type, user); not page.empty();) {
		if (not all.empty())
			pause();
		auto body = get(page);
		// MyAnimeList answers 403 or 404 for a private list or no such user.
		if (not body and (body.error().status == 403 or body.error().status == 404))
			return web::refused(std::format("MyAnimeList has no public list of {}", user));
		if (not body)
			return std::unexpected(body.error());
		auto entries = parseList(*body, type, page);
		if (not entries.has_value())
			return entries;
		all.insert(all.end(), entries.value().begin(), entries.value().end());
	}
	return all;
}

web::answer<std::optional<web::siteEntry>> tracker::entryOf(const entry& title,
                                                            const account& login) {
	if (title.malId <= 0)
		return web::notLinked(provider::kMyAnimeList);
	auto body = web::checked(
	    net_.fetch({"GET", entryUrl(title.type, title.malId), {web::bearer(login.token)}, ""}),
	    provider::kMyAnimeList);
	if (not body and body.error().status == 404)
		return web::refused("MyAnimeList does not know the title");
	if (not body)
		return std::unexpected(body.error());
	return parseEntry(*body, title.type);
}

web::answer<void> tracker::push(const entry& title, const account& login) {
	if (title.malId <= 0)
		return web::notLinked(provider::kMyAnimeList);
	auto body = web::checked(net_.fetch(
	    {"PATCH",
	     std::format("{}/{}/{}/my_list_status", kApiUrl, typeKey(title.type), title.malId),
	     {"Content-Type: application/x-www-form-urlencoded", web::bearer(login.token)},
	     listStatusForm(title)}), provider::kMyAnimeList);
	if (not body)
		return std::unexpected(body.error());
	return {};
}

web::answer<void> tracker::remove(mediaType type, int siteId, const account& login) {
	auto body = web::checked(
	    net_.fetch({"DELETE",
	                std::format("{}/{}/{}/my_list_status", kApiUrl, typeKey(type), siteId),
	                {web::bearer(login.token)},
	                ""}),
	    provider::kMyAnimeList);
	// 404: not on the list, so gone already.
	if (not body and body.error().status != 404)
		return std::unexpected(body.error());
	return {};
}

std::string tracker::cannotAsk() const {
	if (not net_.ids.mal.empty())
		return "";
	return "MyAnimeList answers only programs with a client id: register one at "
	       "https://myanimelist.net/apiconfig and set CATALIST_MAL_CLIENT_ID";
}

web::answer<std::string> tracker::get(const std::string& url) const {
	return web::checked(net_.fetch({"GET", url, {"X-MAL-CLIENT-ID: " + net_.ids.mal}, ""}),
	                    provider::kMyAnimeList);
}

web::answer<std::vector<web::match>> tracker::search(mediaType type, const std::string& query) {
	if (auto why = cannotAsk(); not why.empty())
		return web::refused(why);
	auto body = get(searchUrl(type, query));
	if (not body)
		return std::unexpected(body.error());
	return parseSearch(*body, type);
}

web::answer<web::details> tracker::lookUp(const entry& known) {
	if (auto why = cannotAsk(); not why.empty())
		return web::refused(why);
	if (known.malId <= 0)
		return web::refused("the title has no id to look up");
	auto body = get(titleUrl(known.type, known.malId));
	if (not body and body.error().status == 404)
		return web::refused("MyAnimeList does not know the title");
	if (not body)
		return std::unexpected(body.error());
	return parseTitle(*body, known.type);
}

web::answer<web::loginStart> tracker::startLogin() {
	if (auto why = cannotAsk(); not why.empty())
		return web::refused(why);
	// PKCE's plain method: the challenge is the verifier itself.
	verifier_ = net_.random(64);
	return web::loginStart{
	    std::format("https://myanimelist.net/v1/oauth2/authorize?response_type=code&client_id={}"
	                "&code_challenge={}&code_challenge_method=plain&state=catalist",
	                net_.ids.mal, verifier_),
	    "Open this page and let Catalist in. Your browser is then sent to an\n"
	    "address that may not load: copy that address.",
	    {{"Address", false}}};
}

web::answer<account> tracker::accountFor(const std::optional<web::reply>& reply) {
	auto body = web::checked(reply, provider::kMyAnimeList);
	if (not body and body.error().why == web::failure::kind::kUnreachable)
		return std::unexpected(body.error());
	auto token = body ? web::parseTokenReply(*body) : std::nullopt;
	if (not token)
		return web::refused("MyAnimeList did not take the login");
	auto user = web::checked(
	    net_.fetch({"GET", kApiUrl + "/users/@me", {web::bearer(token->token)}, ""}),
	    provider::kMyAnimeList);
	auto me = user ? json::parse(*user) : std::unexpected("");
	auto name = me.has_value() ? me.value()["name"].asString() : std::nullopt;
	if (not name)
		return web::refused("MyAnimeList did not say whose login it is");
	return account{
	    provider::kMyAnimeList, *name,          std::to_string(me.value()["id"].asCount()),
	    token->token,           token->renewal, net_.now() + token->lifetime};
}

web::answer<account> tracker::finishLogin(const std::vector<std::string>& answers) {
	std::string pasted = answers.empty() ? "" : answers[0];
	// The whole address, or only its code.
	std::string code = web::queryValue(pasted, "code").value_or(pasted);
	if (code.empty() or verifier_.empty())
		return web::refused("no code given");
	return accountFor(net_.fetch({"POST",
	                              kTokenUrl,
	                              {"Content-Type: application/x-www-form-urlencoded"},
	                              web::formEncoded({{"client_id", net_.ids.mal},
	                                                {"grant_type", "authorization_code"},
	                                                {"code", code},
	                                                {"code_verifier", verifier_}})}));
}

web::answer<account> tracker::current(const account& login) {
	if (not web::runningOut(login, net_.now()))
		return login;
	auto renewed = accountFor(net_.fetch({"POST",
	                                      kTokenUrl,
	                                      {"Content-Type: application/x-www-form-urlencoded"},
	                                      web::formEncoded({{"client_id", net_.ids.mal},
	                                                        {"grant_type", "refresh_token"},
	                                                        {"refresh_token", login.renewal}})}));
	if (not renewed and renewed.error().why == web::failure::kind::kUnreachable)
		return renewed;
	if (not renewed)
		return std::unexpected(web::failure{
		    web::failure::kind::kLoginRefused,
		    "your MyAnimeList login has run out: log in again with 'catalist login mal'"});
	return renewed;
}

}  // namespace catalist::mal
