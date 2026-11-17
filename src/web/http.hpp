#ifndef CATALIST_WEB_HTTP_HPP_
#define CATALIST_WEB_HTTP_HPP_

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// What the sites are asked through: one HTTP request, and the function that
// sends it. Plain data, so the sites can be tested without a network.
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

// Text as it may stand in a URL's query: letters, digits and -._~ as they
// are, every other byte as %XX.
[[nodiscard]] std::string UrlEncoded(const std::string& text);

// The body of an application/x-www-form-urlencoded request.
[[nodiscard]] std::string FormEncoded(
    const std::vector<std::pair<std::string, std::string>>& fields);

// The value of `name` in a URL's query ("…?code=abc&state=x" gives "abc"
// for code), decoded; nothing when it has none.
[[nodiscard]] std::optional<std::string> QueryValue(const std::string& url,
                                                    const std::string& name);

}  // namespace catalist::web

#endif  // CATALIST_WEB_HTTP_HPP_
