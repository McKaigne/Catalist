#include "web/tracker.hpp"

#include <format>
#include <utility>

#include "web/json.hpp"

namespace catalist::web {

std::unexpected<failure> unreachable(std::string message) {
	return std::unexpected(failure{failure::kind::kUnreachable, std::move(message)});
}

std::unexpected<failure> notLinked(provider site) {
	return std::unexpected(
	    failure{failure::kind::kNotLinked, "not linked to " + providerName(site)});
}

std::unexpected<failure> refused(std::string message) {
	return std::unexpected(failure{failure::kind::kRefused, std::move(message)});
}

double outOfTen(provider site, int score) {
	switch (site) {
		case provider::kAniList:
			return score / 10.0;
		case provider::kKitsu:
			return score / 2.0;
		case provider::kMyAnimeList:
			break;
	}
	return score;
}

answer<std::string> checked(const std::optional<reply>& got, provider site) {
	std::string name = providerName(site);
	if (not got)
		return unreachable("could not reach " + name);
	int status = got->status;
	if (status >= 200 and status < 300)
		return got->body;
	failure why{failure::kind::kRefused, std::format("{} said no (HTTP {})", name, status), status};
	if (status == 429 or status >= 500) {
		why.why = failure::kind::kUnreachable;
		why.message = std::format("{} is busy or down (HTTP {})", name, status);
	} else if (status == 401 or status == 403) {
		why.why = failure::kind::kLoginRefused;
		why.message = std::format("{} did not take your login; log in again with 'catalist login {}'",
		                          name, providerKey(site));
	} else if (status == 404) {
		why.message = std::format("{} does not know it (HTTP 404)", name);
	}
	return std::unexpected(why);
}

std::vector<answer<details>> iTracker::lookUpMany(const std::vector<entry>& known) {
	std::vector<answer<details>> found;
	for (const auto& each : known) {
		if (not found.empty() and not found.back().has_value() and
		    found.back().error().why == failure::kind::kUnreachable) {
			found.push_back(found.back());
			continue;
		}
		if (not found.empty())
			pause();
		found.push_back(lookUp(each));
	}
	return found;
}

std::optional<tokenReply> parseTokenReply(const std::string& body) {
	auto reply = json::parse(body);
	if (not reply.has_value())
		return std::nullopt;
	tokenReply found;
	found.token = reply.value()["access_token"].asString().value_or("");
	found.renewal = reply.value()["refresh_token"].asString().value_or("");
	found.lifetime = reply.value()["expires_in"].asInteger().value_or(0);
	return found.token.empty() ? std::nullopt : std::optional<tokenReply>(found);
}

std::string bearer(const std::string& token) {
	return "Authorization: Bearer " + token;
}

std::optional<isoDate> dayOf(const std::string& text) {
	return text.size() < 10 ? std::nullopt : isoDate::parse(text.substr(0, 10));
}

bool runningOut(const account& login, long long now) {
	return login.expires > 0 and now + 60 >= login.expires;
}

}  // namespace catalist::web
