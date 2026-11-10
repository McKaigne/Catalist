#ifndef CATALIST_PLATFORM_HTTP_CLIENT_HPP_
#define CATALIST_PLATFORM_HTTP_CLIENT_HPP_

#include <optional>
#include <string>

namespace catalist {

// The only way Catalist reaches the network: one GET for the title search.
class iHttpClient {
public:
  virtual ~iHttpClient() = default;
  // The reply body, or nothing when the request failed for any reason: no
  // connection, a timeout or an HTTP error status.
  [[nodiscard]] virtual std::optional<std::string> get(
      const std::string& url) = 0;
};

// Runs the curl program, so Catalist itself links no network library.
class curlHttpClient : public iHttpClient {
public:
  [[nodiscard]] std::optional<std::string> get(const std::string& url) override;
};

}  // namespace catalist

#endif  // CATALIST_PLATFORM_HTTP_CLIENT_HPP_
