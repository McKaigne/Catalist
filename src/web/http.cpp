#include "web/http.hpp"

#include <fmt/format.h>

#include <cctype>

namespace catalist::web {

std::string urlEncoded(const std::string& text) {
	std::string encoded;
	for (unsigned char c : text) {
		if (std::isalnum(c) or c == '-' or c == '.' or c == '_' or c == '~')
			encoded += static_cast<char>(c);
		else
			encoded += fmt::format("%{:02X}", c);
	}
	return encoded;
}

}  // namespace catalist::web
