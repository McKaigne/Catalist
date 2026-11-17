#include "web/tracker.hpp"

#include <format>
#include <utility>

#include "web/json.hpp"

namespace catalist::web {

std::unexpected<Failure> Unreachable(std::string message) {
	return std::unexpected(Failure{Failure::Kind::kUnreachable, std::move(message)});
}

std::unexpected<Failure> NotLinked(Provider site) {
	return std::unexpected(
	    Failure{Failure::Kind::kNotLinked, "not linked to " + ProviderName(site)});
}

std::unexpected<Failure> Refused(std::string message) {
	return std::unexpected(Failure{Failure::Kind::kRefused, std::move(message)});
}

double OutOfTen(Provider site, int score) {
	switch (site) {
		case Provider::kAniList:
			return score / 10.0;
		case Provider::kKitsu:
			return score / 2.0;
		case Provider::kMyAnimeList:
			break;
	}
	return score;
}

Answer<std::string> Checked(const std::optional<Reply>& got, Provider site) {
	std::string name = ProviderName(site);
	if (not got)
		return Unreachable("could not reach " + name);
	int status = got->status;
	if (status >= 200 and status < 300)
		return got->body;
	Failure why{Failure::Kind::kRefused, std::format("{} said no (HTTP {})", name, status), status};
	if (status == 429 or status >= 500) {
		why.why = Failure::Kind::kUnreachable;
		why.message = std::format("{} is busy or down (HTTP {})", name, status);
	} else if (status == 401 or status == 403) {
		why.why = Failure::Kind::kLoginRefused;
		why.message =
		    std::format("{} did not take your login; log in again with 'catalist login {}'", name,
		                ProviderKey(site));
	} else if (status == 404) {
		why.message = std::format("{} does not know it (HTTP 404)", name);
	}
	return std::unexpected(why);
}

std::vector<Answer<Details>> Tracker::LookUpMany(const std::vector<Entry>& known) {
	std::vector<Answer<Details>> found;
	for (const auto& each : known) {
		if (not found.empty() and not found.back().has_value() and
		    found.back().error().why == Failure::Kind::kUnreachable) {
			found.push_back(found.back());
			continue;
		}
		if (not found.empty())
			Pause();
		found.push_back(LookUp(each));
	}
	return found;
}

std::optional<TokenReply> ParseTokenReply(const std::string& body) {
	auto reply = json::Parse(body);
	if (not reply.has_value())
		return std::nullopt;
	TokenReply found;
	found.token = reply.value()["access_token"].AsString().value_or("");
	found.renewal = reply.value()["refresh_token"].AsString().value_or("");
	found.lifetime = reply.value()["expires_in"].AsInteger().value_or(0);
	return found.token.empty() ? std::nullopt : std::optional<TokenReply>(found);
}

std::string Bearer(const std::string& token) {
	return "Authorization: Bearer " + token;
}

std::optional<IsoDate> DayOf(const std::string& text) {
	return text.size() < 10 ? std::nullopt : IsoDate::Parse(text.substr(0, 10));
}

bool RunningOut(const Account& login, long long now) {
	return login.expires > 0 and now + 60 >= login.expires;
}

}  // namespace catalist::web
