#include "web/mal.hpp"

#include <format>

#include "web/json.hpp"

namespace catalist::mal {

namespace {

std::string FieldsOf(MediaType type) {
	return type == MediaType::kAnime ? "num_episodes,start_date,mean,status"
	                                 : "num_chapters,num_volumes,start_date,mean,status";
}

std::string ListStatus(WatchStatus status, MediaType type) {
	bool anime = type == MediaType::kAnime;
	switch (status) {
		case WatchStatus::kWatching:
			return anime ? "watching" : "reading";
		case WatchStatus::kCompleted:
			return "completed";
		case WatchStatus::kOnHold:
			return "on_hold";
		case WatchStatus::kDropped:
			return "dropped";
		case WatchStatus::kPlanToWatch:
			return anime ? "plan_to_watch" : "plan_to_read";
		// With is_rewatching or is_rereading (see ListStatusForm).
		case WatchStatus::kRewatching:
			return anime ? "watching" : "reading";
	}
	return anime ? "plan_to_watch" : "plan_to_read";
}

std::optional<WatchStatus> StatusOf(const std::string& list_status) {
	if (list_status == "watching" or list_status == "reading")
		return WatchStatus::kWatching;
	if (list_status == "completed")
		return WatchStatus::kCompleted;
	if (list_status == "on_hold")
		return WatchStatus::kOnHold;
	if (list_status == "dropped")
		return WatchStatus::kDropped;
	if (list_status == "plan_to_watch" or list_status == "plan_to_read")
		return WatchStatus::kPlanToWatch;
	return std::nullopt;
}

web::Match MatchOf(const json::Value& node, MediaType type) {
	web::Match found;
	found.mal_id = node["id"].AsCount();
	found.name = node["title"].AsString().value_or("");
	found.total_units = node[type == MediaType::kAnime ? "num_episodes" : "num_chapters"].AsCount();
	found.total_volumes = node["num_volumes"].AsCount();
	found.year = node["start_date"].AsString().value_or("").substr(0, 4);
	found.score = node["mean"].AsNumber().value_or(0.0);
	return found;
}

}  // namespace

std::string SearchUrl(MediaType type, const std::string& query) {
	return std::format("{}/{}?q={}&limit=10&fields={}", kApiUrl, TypeKey(type),
	                   web::UrlEncoded(query), FieldsOf(type));
}

web::Answer<std::vector<web::Match>> ParseSearch(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	const json::Value& data = reply.has_value() ? reply.value()["data"] : json::Value();
	if (not data.IsArray())
		return web::Refused("unexpected reply from MyAnimeList");
	std::vector<web::Match> found;
	for (const auto& item : data.Items()) {
		auto match = MatchOf(item["node"], type);
		if (match.mal_id > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return found;
}

std::string TitleUrl(MediaType type, int mal_id) {
	return std::format("{}/{}/{}?fields={}", kApiUrl, TypeKey(type), mal_id, FieldsOf(type));
}

// The statuses: finished_airing, currently_airing and not_yet_aired for an
// anime; finished, currently_publishing, on_hiatus, discontinued and
// not_yet_published for a manga.
web::Answer<web::Details> ParseTitle(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	if (not reply.has_value() or reply.value()["id"].AsCount() == 0)
		return web::Refused("MyAnimeList does not know the title");
	const json::Value& node = reply.value();
	auto match = MatchOf(node, type);
	std::string status = node["status"].AsString().value_or("");
	web::Details found;
	found.name = match.name;
	found.facts = {match.total_units, match.total_volumes, 0, 0, match.mal_id, 0};
	found.ongoing =
	    status == "currently_airing" or status == "currently_publishing" or status == "on_hiatus";
	if (status == "finished_airing" or status == "finished" or status == "discontinued")
		found.facts.released = match.total_units;
	return found;
}

std::string ListUrl(MediaType type, const std::string& user) {
	bool anime = type == MediaType::kAnime;
	return std::format(
	    "{}/users/{}/{}?fields=list_status{{{}}},{},status&limit=1000&nsfw=true", kApiUrl,
	    web::UrlEncoded(user), anime ? "animelist" : "mangalist",
	    anime ? "status,score,num_episodes_watched,num_times_rewatched,is_rewatching,start_date,"
	            "finish_date"
	          : "status,score,num_chapters_read,num_volumes_read,num_times_reread,is_rereading,"
	            "start_date,finish_date",
	    anime ? "num_episodes" : "num_chapters,num_volumes");
}

web::Answer<std::vector<Entry>> ParseList(const std::string& body, MediaType type,
                                          std::string& next) {
	auto reply = json::Parse(body);
	const json::Value& data = reply.has_value() ? reply.value()["data"] : json::Value();
	if (not data.IsArray())
		return web::Refused("unexpected reply from MyAnimeList");
	bool anime = type == MediaType::kAnime;
	std::vector<Entry> found;
	for (const auto& item : data.Items()) {
		const json::Value& mine = item["list_status"];
		// A rewatch is a rewatch whatever the status says, as Mihon reads it.
		bool again = mine[anime ? "is_rewatching" : "is_rereading"].AsBool().value_or(false);
		auto status = again ? std::optional(WatchStatus::kRewatching)
		                    : StatusOf(mine["status"].AsString().value_or(""));
		auto match = MatchOf(item["node"], type);
		if (not status or match.mal_id == 0 or match.name.empty())
			continue;
		Entry e;
		e.type = type;
		e.title = match.name;
		e.mal_id = match.mal_id;
		e.total_units = match.total_units;
		e.total_volumes = match.total_volumes;
		std::string airing = item["node"]["status"].AsString().value_or("");
		if (airing == "finished_airing" or airing == "finished")
			e.released = match.total_units;
		e.status = *status;
		e.score = mine["score"].AsCount();
		e.progress = mine[anime ? "num_episodes_watched" : "num_chapters_read"].AsCount();
		e.volumes = mine["num_volumes_read"].AsCount();
		e.rewatched = mine[anime ? "num_times_rewatched" : "num_times_reread"].AsCount();
		e.start_date = web::DayOf(mine["start_date"].AsString().value_or(""));
		e.finish_date = web::DayOf(mine["finish_date"].AsString().value_or(""));
		found.push_back(std::move(e));
	}
	next = reply.value()["paging"]["next"].AsString().value_or("");
	return found;
}

std::string EntryUrl(MediaType type, int mal_id) {
	bool anime = type == MediaType::kAnime;
	return std::format(
	    "{}/{}/{}?fields=my_list_status{{{}}}", kApiUrl, TypeKey(type), mal_id,
	    anime ? "status,score,num_episodes_watched,is_rewatching,num_times_rewatched,start_date,"
	            "finish_date"
	          : "status,score,num_chapters_read,num_volumes_read,is_rereading,num_times_reread,"
	            "start_date,finish_date");
}

web::Answer<std::optional<web::SiteEntry>> ParseEntry(const std::string& body, MediaType type) {
	auto reply = json::Parse(body);
	if (not reply.has_value() or reply.value()["id"].AsCount() == 0)
		return web::Refused("unexpected reply from MyAnimeList");
	const json::Value& mine = reply.value()["my_list_status"];
	if (mine.IsNull())
		return std::optional<web::SiteEntry>();
	bool anime = type == MediaType::kAnime;
	bool again = mine[anime ? "is_rewatching" : "is_rereading"].AsBool().value_or(false);
	web::SiteEntry found;
	found.status =
	    again
	        ? WatchStatus::kRewatching
	        : StatusOf(mine["status"].AsString().value_or("")).value_or(WatchStatus::kPlanToWatch);
	found.score = mine["score"].AsCount();
	found.progress = mine[anime ? "num_episodes_watched" : "num_chapters_read"].AsCount();
	found.volumes = mine["num_volumes_read"].AsCount();
	found.rewatched = mine[anime ? "num_times_rewatched" : "num_times_reread"].AsCount();
	found.start_date = web::DayOf(mine["start_date"].AsString().value_or(""));
	found.finish_date = web::DayOf(mine["finish_date"].AsString().value_or(""));
	return found;
}

std::string ListStatusForm(const Entry& title, unsigned fields) {
	bool anime = title.type == MediaType::kAnime;
	std::vector<std::pair<std::string, std::string>> form;
	auto add = [&form, fields](unsigned field, const std::string& name, const std::string& value) {
		if (fields & field)
			form.emplace_back(name, value);
	};
	add(site_field::kStatus, "status", ListStatus(title.status, title.type));
	add(site_field::kScore, "score", std::to_string(title.score));
	std::string again = title.status == WatchStatus::kRewatching ? "true" : "false";
	add(site_field::kStatus, anime ? "is_rewatching" : "is_rereading", again);
	add(site_field::kProgress, anime ? "num_watched_episodes" : "num_chapters_read",
	    std::to_string(title.progress));
	if (not anime)
		add(site_field::kVolumes, "num_volumes_read", std::to_string(title.volumes));
	add(site_field::kRewatched, anime ? "num_times_rewatched" : "num_times_reread",
	    std::to_string(title.rewatched));
	if (title.start_date)
		add(site_field::kStarted, "start_date", title.start_date->ToString());
	if (title.finish_date)
		add(site_field::kFinished, "finish_date", title.finish_date->ToString());
	return web::FormEncoded(form);
}

web::Answer<std::vector<Entry>> Tracker::Pull(MediaType type, const std::string& user) {
	if (auto why = CannotAsk(); not why.empty())
		return web::Refused(why);
	std::vector<Entry> all;
	bool first = true;
	for (std::string page = ListUrl(type, user); not page.empty(); first = false) {
		if (not first)
			Pause();
		auto body = Get(page);
		// MyAnimeList answers 403 or 404 for a private list or no such user.
		if (not body and (body.error().status == 403 or body.error().status == 404))
			return web::Refused(std::format("MyAnimeList has no public list of {}", user));
		if (not body)
			return std::unexpected(body.error());
		auto entries = ParseList(*body, type, page);
		if (not entries.has_value())
			return entries;
		all.insert(all.end(), entries.value().begin(), entries.value().end());
	}
	return all;
}

web::Answer<std::optional<web::SiteEntry>> Tracker::EntryOf(const Entry& title,
                                                            const Account& login) {
	if (title.mal_id <= 0)
		return web::NotLinked(Provider::kMyAnimeList);
	auto body = web::Checked(
	    net_.fetch({"GET", EntryUrl(title.type, title.mal_id), {web::Bearer(login.token)}, ""}),
	    Provider::kMyAnimeList);
	if (not body and body.error().status == 404)
		return web::Refused("MyAnimeList does not know the title");
	if (not body)
		return std::unexpected(body.error());
	return ParseEntry(*body, title.type);
}

// MyAnimeList has no hidden entries: kHidden is not sent.
web::Answer<void> Tracker::Push(const Entry& title, const Account& login,
                                const web::Sending& what) {
	if (title.mal_id <= 0)
		return web::NotLinked(Provider::kMyAnimeList);
	auto body = web::Checked(
	    net_.fetch(
	        {"PATCH",
	         std::format("{}/{}/{}/my_list_status", kApiUrl, TypeKey(title.type), title.mal_id),
	         {"Content-Type: application/x-www-form-urlencoded", web::Bearer(login.token)},
	         ListStatusForm(title, what.fields)}),
	    Provider::kMyAnimeList);
	if (not body)
		return std::unexpected(body.error());
	return {};
}

web::Answer<void> Tracker::Remove(MediaType type, int site_id, const Account& login) {
	auto body = web::Checked(
	    net_.fetch({"DELETE",
	                std::format("{}/{}/{}/my_list_status", kApiUrl, TypeKey(type), site_id),
	                {web::Bearer(login.token)},
	                ""}),
	    Provider::kMyAnimeList);
	// 404: not on the list, so gone already.
	if (not body and body.error().status != 404)
		return std::unexpected(body.error());
	return {};
}

std::string Tracker::CannotAsk() const {
	if (not net_.ids.mal.empty())
		return "";
	return "MyAnimeList answers only programs with a client id: register one at "
	       "https://myanimelist.net/apiconfig and set CATALIST_MAL_CLIENT_ID";
}

web::Answer<std::string> Tracker::Get(const std::string& url) const {
	return web::Checked(net_.fetch({"GET", url, {"X-MAL-CLIENT-ID: " + net_.ids.mal}, ""}),
	                    Provider::kMyAnimeList);
}

web::Answer<std::vector<web::Match>> Tracker::Search(MediaType type, const std::string& query) {
	if (auto why = CannotAsk(); not why.empty())
		return web::Refused(why);
	auto body = Get(SearchUrl(type, query));
	if (not body)
		return std::unexpected(body.error());
	return ParseSearch(*body, type);
}

web::Answer<web::Details> Tracker::LookUp(const Entry& known) {
	if (auto why = CannotAsk(); not why.empty())
		return web::Refused(why);
	if (known.mal_id <= 0)
		return web::Refused("the title has no id to look up");
	auto body = Get(TitleUrl(known.type, known.mal_id));
	if (not body and body.error().status == 404)
		return web::Refused("MyAnimeList does not know the title");
	if (not body)
		return std::unexpected(body.error());
	return ParseTitle(*body, known.type);
}

web::Answer<web::LoginStart> Tracker::StartLogin() {
	if (auto why = CannotAsk(); not why.empty())
		return web::Refused(why);
	// PKCE's plain method: the challenge is the verifier itself.
	verifier_ = net_.random(64);
	return web::LoginStart{
	    std::format("https://myanimelist.net/v1/oauth2/authorize?response_type=code&client_id={}"
	                "&code_challenge={}&code_challenge_method=plain&state=catalist",
	                net_.ids.mal, verifier_),
	    "Open this page and let Catalist in. Your browser is then sent to an\n"
	    "address that may not load: copy that address.",
	    {{"Address", false}}};
}

web::Answer<Account> Tracker::AccountFor(const std::optional<web::Reply>& reply) {
	auto body = web::Checked(reply, Provider::kMyAnimeList);
	if (not body and body.error().why == web::Failure::Kind::kUnreachable)
		return std::unexpected(body.error());
	auto token = body ? web::ParseTokenReply(*body) : std::nullopt;
	if (not token)
		return web::Refused("MyAnimeList did not take the login");
	auto user =
	    web::Checked(net_.fetch({"GET", kApiUrl + "/users/@me", {web::Bearer(token->token)}, ""}),
	                 Provider::kMyAnimeList);
	auto me = user ? json::Parse(*user) : std::unexpected("");
	auto name = me.has_value() ? me.value()["name"].AsString() : std::nullopt;
	if (not name)
		return web::Refused("MyAnimeList did not say whose login it is");
	return Account{
	    Provider::kMyAnimeList, *name,          std::to_string(me.value()["id"].AsCount()),
	    token->token,           token->renewal, net_.now() + token->lifetime};
}

web::Answer<Account> Tracker::FinishLogin(const std::vector<std::string>& answers) {
	std::string pasted = answers.empty() ? "" : answers[0];
	// The whole address, or only its code.
	std::string code = web::QueryValue(pasted, "code").value_or(pasted);
	if (code.empty() or verifier_.empty())
		return web::Refused("no code given");
	return AccountFor(net_.fetch({"POST",
	                              kTokenUrl,
	                              {"Content-Type: application/x-www-form-urlencoded"},
	                              web::FormEncoded({{"client_id", net_.ids.mal},
	                                                {"grant_type", "authorization_code"},
	                                                {"code", code},
	                                                {"code_verifier", verifier_}})}));
}

web::Answer<Account> Tracker::Current(const Account& login) {
	if (not web::RunningOut(login, net_.now()))
		return login;
	auto renewed = AccountFor(net_.fetch({"POST",
	                                      kTokenUrl,
	                                      {"Content-Type: application/x-www-form-urlencoded"},
	                                      web::FormEncoded({{"client_id", net_.ids.mal},
	                                                        {"grant_type", "refresh_token"},
	                                                        {"refresh_token", login.renewal}})}));
	if (not renewed and renewed.error().why == web::Failure::Kind::kUnreachable)
		return renewed;
	if (not renewed)
		return std::unexpected(web::Failure{
		    web::Failure::Kind::kLoginRefused,
		    "your MyAnimeList login has run out: log in again with 'catalist login mal'"});
	return renewed;
}

}  // namespace catalist::mal
