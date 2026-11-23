#ifndef CATALIST_WEB_HTTP_HPP_
#define CATALIST_WEB_HTTP_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

// What AniList is asked through: one HTTP request, and the function that
// sends it. Plain data, so it can be tested without a network.
namespace catalist::web {

// A GET unless said otherwise.
struct Request {
	std::string method = "GET";
	std::string url;
	// "Name: value" each.
	std::vector<std::string> headers{};
	std::string body{};
};

// What a site answered: the HTTP status and the body, whatever the status.
struct Reply {
	int status = 200;
	std::string body{};
};

// Sends a request: the reply, or nothing when there was none (no
// connection, a timeout). The program passes HttpFetch; tests pass a fake.
using Fetcher = std::function<std::optional<Reply>(const Request&)>;

}  // namespace catalist::web

#endif  // CATALIST_WEB_HTTP_HPP_
