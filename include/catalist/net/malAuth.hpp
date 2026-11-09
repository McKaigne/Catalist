#ifndef CATALIST_NET_MAL_AUTH_HPP_
#define CATALIST_NET_MAL_AUTH_HPP_

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/platform/processLauncher.hpp"

namespace catalist {

// Receives the browser redirect that carries the OAuth authorization code.
class iAuthCallbackServer {
 public:
  virtual ~iAuthCallbackServer() = default;
  [[nodiscard]] virtual std::optional<std::string> waitForCode(
      std::chrono::seconds timeout) = 0;
};

// Listens on 127.0.0.1 only: the redirect comes from the user's own browser,
// and binding every interface would let other hosts race to deliver a code.
class loopbackAuthServer : public iAuthCallbackServer {
 public:
  explicit loopbackAuthServer(std::uint16_t port) : port_(port) {}
  [[nodiscard]] std::optional<std::string> waitForCode(
      std::chrono::seconds timeout) override;

 private:
  std::uint16_t port_;
};

// Extracts and percent-decodes the `code` query parameter from the raw HTTP
// request the browser sends to the loopback server.
[[nodiscard]] std::optional<std::string> parseAuthCode(
    const std::string& httpRequestText);

struct malCredentials {
  std::string accessToken;
  std::string refreshToken;
  long long expiresAt = 0;
};

class malAuth {
 public:
  malAuth(iFileSystem& fileSystem, iHttpClient& http,
          iProcessLauncher& launcher, iAuthCallbackServer& callbackServer,
          const iClock& clock, std::filesystem::path credentialsPath);

  [[nodiscard]] bool isAuthenticated() const;
  bool authenticate(std::string& outMsg);
  void logout();

  [[nodiscard]] std::string getAccessToken() const;
  void load();
  void save();

 private:
  bool exchangeToken(const std::string& code, const std::string& verifier,
                     std::string& outMsg);

  iFileSystem& fileSystem_;
  iHttpClient& http_;
  iProcessLauncher& launcher_;
  iAuthCallbackServer& callbackServer_;
  const iClock& clock_;
  std::filesystem::path credentialsPath_;
  malCredentials creds_;
};


}  // namespace catalist

#endif  // CATALIST_NET_MAL_AUTH_HPP_
