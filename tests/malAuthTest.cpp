#include <doctest/doctest.h>

#include <optional>
#include <string>

#include "catalist/core/clock.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/platform/processLauncher.hpp"
#include "catalist/sync.hpp"
#include "support/testSupport.hpp"

using catalist::parseAuthCode;

namespace {

class scriptedAuthServer : public catalist::iAuthCallbackServer {
public:
  explicit scriptedAuthServer(std::optional<std::string> code)
      : code_(std::move(code)) {}
  std::optional<std::string> waitForCode(std::chrono::seconds) override {
    return code_;
  }

private:
  std::optional<std::string> code_;
};

struct authFixture {
  catalist::memoryFileSystem fs;
  catalist::mockHttpClient http;
  catalist::fakeProcessLauncher launcher;
  catalist::fixedClock clock = catalist::test::makeTestClock();
  std::filesystem::path credentials = "/config/mal_auth.json";

  bool authenticateWith(std::optional<std::string> code, std::string& message) {
    scriptedAuthServer server(std::move(code));
    catalist::malAuth auth(fs, http, launcher, server, clock, credentials);
    return auth.authenticate(message);
  }
};

const char* kTokenResponse =
    R"({"token_type":"Bearer","expires_in":2678400,"access_token":"tok","refresh_token":"ref"})";

std::string queryValue(const std::string& body, const std::string& key) {
  std::string marker = key + "=";
  size_t start = body.find(marker);
  if (start == std::string::npos)
    return "<missing>";
  start += marker.size();
  return body.substr(start, body.find('&', start) - start);
}

}  // namespace

TEST_CASE_FIXTURE(authFixture, "malAuth hostile code is form encoded and never reaches a shell") {
  http.setResponse("myanimelist.net/v1/oauth2/token", {200, kTokenResponse, ""});
  const std::string hostileCode =
      "x\"; touch /tmp/pwned; echo '$(id)'&grant_type=evil\nHost: x";

  std::string message;
  REQUIRE(authenticateWith(hostileCode, message));
  CHECK(message == "Successfully authenticated with MyAnimeList.");

  REQUIRE(http.requests().size() == 1);
  const auto& request = http.requests()[0];
  CHECK(request.method == "POST");
  CHECK(request.headers.at("Content-Type") ==
        "application/x-www-form-urlencoded");
  CHECK(queryValue(request.body, "code") ==
        "x%22%3b%20touch%20%2ftmp%2fpwned%3b%20echo%20%27%24%28id%29%27%26"
        "grant_type%3devil%0aHost%3a%20x");
  CHECK(request.body.find("&grant_type=authorization_code") !=
        std::string::npos);
  CHECK(request.body.find("grant_type=evil") == std::string::npos);

  REQUIRE(launcher.calls().size() == 1);
  CHECK(launcher.calls()[0].detached);
  CHECK(launcher.calls()[0].argv.at(0) == "xdg-open");
}

TEST_CASE_FIXTURE(authFixture, "malAuth stores tokens owner-only with expiry from the clock") {
  http.setResponse("oauth2/token", {200, kTokenResponse, ""});
  std::string message;
  REQUIRE(authenticateWith(std::string("good-code"), message));

  auto saved = fs.readFile(credentials);
  REQUIRE(saved.has_value());
  CHECK(saved->find("\"access_token\": \"tok\"") != std::string::npos);
  CHECK(saved->find("\"expires_at\": " + std::to_string(1792497600LL + 2678400)) !=
        std::string::npos);
  CHECK(fs.isOwnerOnly(credentials));

  scriptedAuthServer unused(std::nullopt);
  catalist::malAuth reloaded(fs, http, launcher, unused, clock, credentials);
  CHECK(reloaded.isAuthenticated());
  CHECK(reloaded.getAccessToken() == "tok");
}

TEST_CASE_FIXTURE(authFixture, "malAuth reports a timeout without contacting MAL") {
  std::string message;
  CHECK_FALSE(authenticateWith(std::nullopt, message));
  CHECK(message ==
        "Authentication timed out or failed. Did you close the browser?");
  CHECK(http.requests().empty());
}

TEST_CASE_FIXTURE(authFixture, "malAuth rejects a token response without an access token") {
  http.setResponse("oauth2/token", {400, R"({"error":"invalid_grant"})", ""});
  std::string message;
  CHECK_FALSE(authenticateWith(std::string("code"), message));
  CHECK(message == "Failed to exchange authorization code with MyAnimeList.");
  CHECK_FALSE(fs.exists(credentials));
}

TEST_CASE("parseAuthCode reads and decodes the code parameter") {
  CHECK(parseAuthCode("GET /?code=abc123&state=s HTTP/1.1\r\nHost: x\r\n") ==
        "abc123");
  CHECK(parseAuthCode("GET /callback?state=s&code=a%2Bb%20c HTTP/1.1\r\n") ==
        "a+b c");
}

TEST_CASE("parseAuthCode ignores look-alike parameters and bad input") {
  CHECK_FALSE(parseAuthCode("GET /?error_code=1 HTTP/1.1\r\n").has_value());
  CHECK_FALSE(parseAuthCode("GET / HTTP/1.1\r\ncode=abc\r\n").has_value());
  CHECK_FALSE(parseAuthCode("GET /?code= HTTP/1.1\r\n").has_value());
  CHECK_FALSE(parseAuthCode("GET /?code=%zz HTTP/1.1\r\n").has_value());
  CHECK_FALSE(parseAuthCode("").has_value());
}
