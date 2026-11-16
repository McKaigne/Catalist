#include "web/tracker.hpp"

#include "web/json.hpp"

namespace catalist::web {

bool unreachable(const std::string& message) {
	return message.rfind("could not reach ", 0) == 0;
}

std::vector<result<details>> iTracker::lookUpMany(const std::vector<entry>& known) {
	std::vector<result<details>> found;
	for (const auto& each : known) {
		if (not found.empty() and not found.back().ok() and
		    unreachable(found.back().errorMessage())) {
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
	if (not reply.ok())
		return std::nullopt;
	tokenReply found;
	found.token = reply.value()["access_token"].asString().value_or("");
	found.renewal = reply.value()["refresh_token"].asString().value_or("");
	found.lifetime = reply.value()["expires_in"].asInteger().value_or(0);
	return found.token.empty() ? std::nullopt : std::optional<tokenReply>(found);
}

std::string notLinked(provider site) {
	return "not linked to " + providerName(site);
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
