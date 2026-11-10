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

}  // namespace catalist::url
