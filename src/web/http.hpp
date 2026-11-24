#ifndef CATALIST_WEB_HTTP_HPP_
#define CATALIST_WEB_HTTP_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace catalist::web {

struct Request {
	std::string method = "GET";
	std::string url;
	std::vector<std::string> headers{};
	std::string body{};
};

struct Reply {
	int status = 200;
	std::string body{};
};

using Fetcher = std::function<std::optional<Reply>(const Request&)>;

}  // namespace catalist::web

#endif  // CATALIST_WEB_HTTP_HPP_
