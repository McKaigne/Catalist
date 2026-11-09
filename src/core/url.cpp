#include "catalist/core/url.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace catalist::url {

namespace {

bool isUnreserved(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' ||
         c == '.' || c == '~';
}

std::optional<int> hexValue(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return std::nullopt;
}

}  // namespace

std::string encode(std::string_view text) {
  std::ostringstream encoded;
  encoded.fill('0');
  encoded << std::hex;
  for (char c : text) {
    if (isUnreserved(c))
      encoded << c;
    else
      encoded << '%' << std::setw(2)
              << static_cast<int>(static_cast<unsigned char>(c));
  }
  return encoded.str();
}

std::optional<std::string> decode(std::string_view text) {
  std::string decoded;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '+') {
      decoded += ' ';
      continue;
    }
    if (text[i] != '%') {
      decoded += text[i];
      continue;
    }
    if (i + 2 >= text.size())
      return std::nullopt;
    auto high = hexValue(text[i + 1]);
    auto low = hexValue(text[i + 2]);
    if (!high || !low)
      return std::nullopt;
    decoded += static_cast<char>(*high * 16 + *low);
    i += 2;
  }
  return decoded;
}

}  // namespace catalist::url
