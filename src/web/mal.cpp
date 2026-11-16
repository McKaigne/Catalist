#include "web/mal.hpp"

#include <fmt/format.h>

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
	return fmt::format("{}/{}?q={}&limit=10&fields={}", kApiUrl, kindOf(type),
	                   web::urlEncoded(query), fieldsOf(type));
}

result<std::vector<web::match>> parseSearch(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	const json::value& data = reply.ok() ? reply.value()["data"] : json::value();
	if (not data.isArray())
		return result<std::vector<web::match>>::failure("unexpected reply from MyAnimeList");
	std::vector<web::match> found;
	for (const auto& item : data.items()) {
		auto match = matchOf(item["node"], type);
		if (match.malId > 0 and not match.name.empty())
			found.push_back(std::move(match));
	}
	return result<std::vector<web::match>>::success(std::move(found));
}

std::string titleUrl(mediaType type, int malId) {
	return fmt::format("{}/{}/{}?fields={}", kApiUrl, kindOf(type), malId, fieldsOf(type));
}

// The statuses: finished_airing, currently_airing and not_yet_aired for an
// anime; finished, currently_publishing, on_hiatus, discontinued and
// not_yet_published for a manga.
result<web::details> parseTitle(const std::string& body, mediaType type) {
	auto reply = json::parse(body);
	if (not reply.ok() or countOrZero(reply.value()["id"]) == 0)
		return result<web::details>::failure("MyAnimeList does not know the title");
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
	return result<web::details>::success(std::move(found));
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
		return result<std::vector<web::match>>::failure(why);
	auto body = get(searchUrl(type, query));
	if (not body)
		return result<std::vector<web::match>>::failure("could not reach MyAnimeList");
	return parseSearch(*body, type);
}

result<web::details> tracker::lookUp(const entry& known) {
	if (auto why = cannotAsk(); not why.empty())
		return result<web::details>::failure(why);
	if (known.malId <= 0)
		return result<web::details>::failure("the title has no id to look up");
	auto body = get(titleUrl(known.type, known.malId));
	if (not body)
		return result<web::details>::failure("could not reach MyAnimeList");
	return parseTitle(*body, known.type);
}

result<web::loginStart> tracker::startLogin() {
	if (auto why = cannotAsk(); not why.empty())
		return result<web::loginStart>::failure(why);
	// PKCE's plain method: the challenge is the verifier itself.
	verifier_ = net_.random(64);
	return result<web::loginStart>::success(
	    {fmt::format("https://myanimelist.net/v1/oauth2/authorize?response_type=code&client_id={}"
	                 "&code_challenge={}&code_challenge_method=plain&state=catalist",
	                 net_.ids.mal, verifier_),
	     "Open this page and let Catalist in. Your browser is then sent to an\n"
	     "address that may not load: copy that address.",
	     {{"Address", false}}});
}

result<account> tracker::accountFor(const std::optional<std::string>& reply) {
	auto token = reply ? web::parseTokenReply(*reply) : std::nullopt;
	if (not token)
		return result<account>::failure(
		    "MyAnimeList did not take the login, or could not be reached");
	auto body = net_.fetch({"GET", kApiUrl + "/users/@me", {web::bearer(token->token)}, ""});
	auto me = body ? json::parse(*body) : result<json::value>::failure("");
	auto name = me.ok() ? me.value()["name"].asString() : std::nullopt;
	if (not name)
		return result<account>::failure("MyAnimeList did not say whose login it is");
	return result<account>::success({provider::kMyAnimeList, *name,
	                                 std::to_string(countOrZero(me.value()["id"])), token->token,
	                                 token->renewal, net_.now() + token->lifetime});
}

result<account> tracker::finishLogin(const std::vector<std::string>& answers) {
	std::string pasted = answers.empty() ? "" : answers[0];
	// The whole address, or only its code.
	std::string code = web::queryValue(pasted, "code").value_or(pasted);
	if (code.empty() or verifier_.empty())
		return result<account>::failure("no code given");
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
		return result<account>::success(login);
	auto renewed = accountFor(net_.fetch({"POST",
	                                      kTokenUrl,
	                                      {"Content-Type: application/x-www-form-urlencoded"},
	                                      web::formEncoded({{"client_id", net_.ids.mal},
	                                                        {"grant_type", "refresh_token"},
	                                                        {"refresh_token", login.renewal}})}));
	if (not renewed.ok())
		return result<account>::failure(
		    "your MyAnimeList login has run out: log in again with 'catalist login mal'");
	return renewed;
}

}  // namespace catalist::mal
