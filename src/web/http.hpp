#ifndef CATALIST_WEB_HTTP_HPP_
#define CATALIST_WEB_HTTP_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

// What the sites are asked through: one HTTP request, and the function that
// sends it. Plain data, so the sites can be tested without a network.
namespace catalist::web {

// A GET unless said otherwise.
struct request {
	std::string method = "GET";
	std::string url;
	// "Name: value" each.
	std::vector<std::string> headers{};
	std::string body{};
};

// Sends a request: the body of the reply, or nothing when it failed for any
// reason (no connection, a timeout, an HTTP error status). The program
// passes httpFetch; tests pass a fake.
using fetcher = std::function<std::optional<std::string>(const request&)>;

}  // namespace catalist::web

#endif  // CATALIST_WEB_HTTP_HPP_
