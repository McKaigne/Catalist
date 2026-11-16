#include "web/mal.hpp"

#include <format>

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
	return std::format("{}/{}?q={}&limit=10&fields={}", kApiUrl, kindOf(type),
	                   web::urlEncoded(query), fieldsOf(type));
}

result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& data = reply.has_value() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return std::unexpected("unexpected reply from MyAnimeList");
	std::vector<web::match> found;
	for (const auto& item : data.items()) {
		auto match = matchOf(item["node"], type);
		if (match.malId > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return found;
}

std::string titleUrl(mediaType type, int malId) {
	return std::format("{}/{}/{}?fields={}", kApiUrl, kindOf(type), malId, fieldsOf(type));
}

// The statuses: finished_airing, currently_airing and not_yet_aired for an
// anime; finished, currently_publishing, on_hiatus, discontinued and
// not_yet_published for a manga.
result<web::details> parseTitle(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.has_value() or countOrZero(reply.value()["id"]) == 0)
		return std::unexpected("MyAnimeList does not know the title");
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
	    anime ? "status,score,num_episodes_watched,num_times_rewatched,start_date,finish_date"
	          : "status,score,num_chapters_read,num_volumes_read,num_times_reread,start_date,"
	            "finish_date",
	    anime ? "num_episodes" : "num_chapters,num_volumes");
}

result<std::vector<entry>> parseList(const std::string& body, mediaType type, std::string& next) {
	auto reply = json::parse(body);
	const json::value& data = reply.has_value() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return std::unexpected("unexpected reply from MyAnimeList");
	bool anime = type == mediaType::kAnime;
	std::vector<entry> found;
	for (const auto& item : data.items()) {
		const json::value& mine = item["list_status"];
		auto status = statusOf(mine["status"].asString().value_or(""));
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
		e.score = countOrZero(mine["score"]);
		e.progress = countOrZero(mine[anime ? "num_episodes_watched" : "num_chapters_read"]);
		e.volumes = countOrZero(mine["num_volumes_read"]);
		e.rewatched = countOrZero(mine[anime ? "num_times_rewatched" : "num_times_reread"]);
		e.startDate = web::dayOf(mine["start_date"].asString().value_or(""));
		e.finishDate = web::dayOf(mine["finish_date"].asString().value_or(""));
		found.push_back(std::move(e));
	}
	next = reply.value()["paging"]["next"].asString().value_or("");
	return found;
}

std::string listStatusForm(const entry& title) {
	bool anime = title.type == mediaType::kAnime;
	std::vector<std::pair<std::string, std::string>> fields = {
	    {"status", listStatus(title.status, title.type)}, {"score", std::to_string(title.score)}};
	if (anime) {
		fields.emplace_back("num_watched_episodes", std::to_string(title.progress));
		fields.emplace_back("num_times_rewatched", std::to_string(title.rewatched));
	} else {
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

result<std::vector<entry>> tracker::pull(mediaType type, const std::string& user) {
	if (auto why = cannotAsk(); not why.empty())
		return std::unexpected(why);
	std::vector<entry> all;
	for (std::string page = listUrl(type, user); not page.empty();) {
		if (not all.empty())
			pause();
		auto body = get(page);
		// MyAnimeList answers 403 or 404 for a private list or no such user.
		if (not body)
			return std::unexpected(
			    std::format("could not reach MyAnimeList, or it has no public list of {}", user));
		auto entries = parseList(*body, type, page);
		if (not entries.has_value())
			return entries;
		all.insert(all.end(), entries.value().begin(), entries.value().end());
	}
	return all;
}

done tracker::push(const entry& title, const account& login) {
	if (title.malId <= 0)
		return std::unexpected(web::notLinked(provider::kMyAnimeList));
	auto body = net_.fetch(
	    {"PATCH",
	     std::format("{}/{}/{}/my_list_status", kApiUrl, kindOf(title.type), title.malId),
	     {"Content-Type: application/x-www-form-urlencoded", web::bearer(login.token)},
	     listStatusForm(title)});
	if (not body)
		return std::unexpected("could not reach MyAnimeList, or it did not take the change");
	return done{};
}

done tracker::remove(mediaType type, int siteId, const account& login) {
	auto body = net_.fetch({"DELETE",
	                        std::format("{}/{}/{}/my_list_status", kApiUrl, kindOf(type), siteId),
	                        {web::bearer(login.token)},
	                        ""});
	if (not body)
		return std::unexpected("could not reach MyAnimeList, or it did not take the change");
	return done{};
}

std::string tracker::cannotAsk() const {
	if (not net_.ids.mal.empty())
		return "";
	return "MyAnimeList answers only programs with a client id: register one at "
	       "https://myanimelist.net/apiconfig and set CATALIST_MAL_CLIENT_ID";
}

std::optional<std::string> tracker::get(const std::string& url) const {
	return net_.fetch({"GET", url, {"X-MAL-CLIENT-ID: " + net_.ids.mal}, ""});
}

result<std::vector<web::match>> tracker::search(mediaType type, const std::string& query) {
	if (auto why = cannotAsk(); not why.empty())
		return std::unexpected(why);
	auto body = get(searchUrl(type, query));
	if (not body)
		return std::unexpected("could not reach MyAnimeList");
	return parseSearch(*body, type);
}

result<web::details> tracker::lookUp(const entry& known) {
	if (auto why = cannotAsk(); not why.empty())
		return std::unexpected(why);
	if (known.malId <= 0)
		return std::unexpected("the title has no id to look up");
	auto body = get(titleUrl(known.type, known.malId));
	if (not body)
		return std::unexpected("could not reach MyAnimeList");
	return parseTitle(*body, known.type);
}

result<web::loginStart> tracker::startLogin() {
	if (auto why = cannotAsk(); not why.empty())
		return std::unexpected(why);
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

result<account> tracker::accountFor(const std::optional<std::string>& reply) {
	auto token = reply ? web::parseTokenReply(*reply) : std::nullopt;
	if (not token)
		return std::unexpected("MyAnimeList did not take the login, or could not be reached");
	auto body = net_.fetch({"GET", kApiUrl + "/users/@me", {web::bearer(token->token)}, ""});
	auto me = body ? json::parse(*body) : std::unexpected("");
	auto name = me.has_value() ? me.value()["name"].asString() : std::nullopt;
	if (not name)
		return std::unexpected("MyAnimeList did not say whose login it is");
	return account{
	    provider::kMyAnimeList, *name,          std::to_string(countOrZero(me.value()["id"])),
	    token->token,           token->renewal, net_.now() + token->lifetime};
}

result<account> tracker::finishLogin(const std::vector<std::string>& answers) {
	std::string pasted = answers.empty() ? "" : answers[0];
	// The whole address, or only its code.
	std::string code = web::queryValue(pasted, "code").value_or(pasted);
	if (code.empty() or verifier_.empty())
		return std::unexpected("no code given");
	return accountFor(net_.fetch({"POST",
	                              kTokenUrl,
	                              {"Content-Type: application/x-www-form-urlencoded"},
	                              web::formEncoded({{"client_id", net_.ids.mal},
	                                                {"grant_type", "authorization_code"},
	                                                {"code", code},
	                                                {"code_verifier", verifier_}})}));
}

result<account> tracker::current(const account& login) {
	if (not web::runningOut(login, net_.now()))
		return login;
	auto renewed = accountFor(net_.fetch({"POST",
	                                      kTokenUrl,
	                                      {"Content-Type: application/x-www-form-urlencoded"},
	                                      web::formEncoded({{"client_id", net_.ids.mal},
	                                                        {"grant_type", "refresh_token"},
	                                                        {"refresh_token", login.renewal}})}));
	if (not renewed.has_value())
		return std::unexpected(
		    "your MyAnimeList login has run out: log in again with 'catalist login mal'");
	return renewed;
}

}  // namespace catalist::mal
