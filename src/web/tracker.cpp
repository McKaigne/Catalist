#include "web/tracker.hpp"

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
