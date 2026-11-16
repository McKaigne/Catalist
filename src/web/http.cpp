#include "web/http.hpp"

#include <cctype>
#include <format>

namespace catalist::web {

std::string urlEncoded(const std::string& text) {
	std::string encoded;
	for (unsigned char c : text) {
		if (std::isalnum(c) or c == '-' or c == '.' or c == '_' or c == '~')
			encoded += static_cast<char>(c);
		else
			encoded += std::format("%{:02X}", c);
	}
	return encoded;
}

std::string formEncoded(const std::vector<std::pair<std::string, std::string>>& fields) {
	std::string body;
	for (const auto& [name, value] : fields)
		body +=
		    std::format("{}{}={}", body.empty() ? "" : "&", urlEncoded(name), urlEncoded(value));
	return body;
}

std::optional<std::string> queryValue(const std::string& url, const std::string& name) {
	auto query = url.find('?');
	if (query == std::string::npos)
		return std::nullopt;
	size_t from = query + 1;
	while (from <= url.size()) {
		size_t end = url.find_first_of("&#", from);
		std::string pair = url.substr(from, end == std::string::npos ? end : end - from);
		if (pair.rfind(name + "=", 0) == 0) {
			std::string value;
			std::string coded = pair.substr(name.size() + 1);
			for (size_t i = 0; i < coded.size(); ++i) {
				bool escaped = coded[i] == '%' and i + 2 < coded.size() and
				               std::isxdigit(static_cast<unsigned char>(coded[i + 1])) and
				               std::isxdigit(static_cast<unsigned char>(coded[i + 2]));
				if (escaped) {
					value += static_cast<char>(std::stoi(coded.substr(i + 1, 2), nullptr, 16));
					i += 2;
				} else {
					value += coded[i] == '+' ? ' ' : coded[i];
				}
			}
			return value;
		}
		if (end == std::string::npos or url[end] == '#')
			break;
		from = end + 1;
	}
	return std::nullopt;
}

}  // namespace catalist::web
