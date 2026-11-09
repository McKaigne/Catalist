#include "catalist/sync.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <optional>
#include <random>
#include <regex>
#include <thread>

#include "catalist/core/jsonLite.hpp"
#include "catalist/database.hpp"

namespace catalist {

const std::string kMalClientId =
    "6114d00ca681b7701d1e15fe11a4987e";  // Standard open-source fallback client
                                         // ID

namespace {

std::string stringField(const std::string& json, const char* key) {
  return jsonLite::extractJsonString(json, key).value_or("");
}

long long integerField(const std::string& json, const char* key) {
  return static_cast<long long>(
      jsonLite::extractJsonNumber(json, key).value_or(0.0));
}

}  // namespace

// -----------------------------------------------------------------------------
// OAuth 2.0 PKCE & Loopback Server Implementation
// -----------------------------------------------------------------------------

malAuth::malAuth(iFileSystem& fileSystem, iHttpClient& http,
                 iProcessLauncher& launcher, const iClock& clock,
                 std::filesystem::path credentialsPath)
    : fileSystem_(fileSystem),
      http_(http),
      launcher_(launcher),
      clock_(clock),
      credentialsPath_(std::move(credentialsPath)) {
  load();
}

bool malAuth::isAuthenticated() const {
  return !creds_.accessToken.empty();
}

void malAuth::logout() {
  creds_ = malCredentials();
  save();
}

std::string malAuth::getAccessToken() const {
  // TODO: Add refresh token rotation check here before returning
  return creds_.accessToken;
}

void malAuth::load() {
  auto content = fileSystem_.readFile(credentialsPath_);
  if (!content)
    return;
  creds_.accessToken = stringField(*content, "access_token");
  creds_.refreshToken = stringField(*content, "refresh_token");
  creds_.expiresAt = integerField(*content, "expires_at");
}

void malAuth::save() {
  std::string content = "{\n  \"access_token\": \"" + creds_.accessToken +
                        "\",\n  \"refresh_token\": \"" +
                        creds_.refreshToken + "\",\n  \"expires_at\": " +
                        std::to_string(creds_.expiresAt) + "\n}\n";
  if (!fileSystem_.writeFileAtomic(credentialsPath_, content))
    return;
  // Tokens grant account access, so the file must not be world-readable.
  fileSystem_.restrictToOwner(credentialsPath_);
}

static std::string generateCodeVerifier() {
  const std::string charset =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
  std::string verifier;
  for (int i = 0; i < 128; ++i) {
    verifier += charset[dist(gen)];
  }
  return verifier;
}

static std::string waitForAuthCode() {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
    return "";

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    close(server_fd);
    return "";
  }

  if (listen(server_fd, 1) < 0) {
    close(server_fd);
    return "";
  }

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(server_fd, &readfds);
  struct timeval timeout;
  timeout.tv_sec =
      60;  // 60-second timeout to prevent permanently blocking thread
  timeout.tv_usec = 0;

  int activity = select(server_fd + 1, &readfds, nullptr, nullptr, &timeout);
  if (activity <= 0 || !FD_ISSET(server_fd, &readfds)) {
    close(server_fd);
    return "";
  }

  int new_socket = accept(server_fd, nullptr, nullptr);
  if (new_socket < 0) {
    close(server_fd);
    return "";
  }

  char buffer[2048] = {0};
  if (read(new_socket, buffer, 2047) < 0) { /* ignore */
  }
  std::string request(buffer);

  std::string code;
  std::regex codeRegex(R"(code=([^&\s]+))");
  std::smatch match;
  if (std::regex_search(request, match, codeRegex)) {
    code = match[1].str();
  }

  std::string response =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
      "<html><body style='font-family: sans-serif; text-align: center; margin-top: 10%;'>"
      "<h2>Authentication Successful!</h2>"
      "<p>You can securely close this tab and return to your terminal.</p>"
      "</body></html>";
  if (write(new_socket, response.c_str(), response.length()) < 0) { /* ignore */
  }

  close(new_socket);
  close(server_fd);

  return code;
}

bool malAuth::exchangeToken(const std::string& code,
                            const std::string& verifier, std::string& outMsg) {
  httpRequest request;
  request.url = "https://myanimelist.net/v1/oauth2/token";
  request.method = "POST";
  request.headers["Content-Type"] = "application/x-www-form-urlencoded";
  request.body = "client_id=" + kMalClientId + "&code=" + code +
                 "&code_verifier=" + verifier +
                 "&grant_type=authorization_code";

  auto response = http_.execute(request);
  std::string json = response.ok() ? response.value().body : "";
  if (json.find("access_token") == std::string::npos) {
    outMsg = "Failed to exchange authorization code with MyAnimeList.";
    return false;
  }

  creds_.accessToken = stringField(json, "access_token");
  creds_.refreshToken = stringField(json, "refresh_token");
  long long expiresIn = integerField(json, "expires_in");
  auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                        clock_.now().time_since_epoch())
                        .count();
  creds_.expiresAt = nowSeconds + expiresIn;

  save();
  outMsg = "Successfully authenticated with MyAnimeList.";
  return true;
}

bool malAuth::authenticate(std::string& outMsg) {
  std::string verifier = generateCodeVerifier();
  std::string authUrl =
      "https://myanimelist.net/v1/oauth2/authorize?response_type=code&"
      "client_id=" +
      kMalClientId + "&code_challenge=" + verifier +
      "&code_challenge_method=plain";
  launcher_.spawnDetached(browserOpenCommand(authUrl));

  std::string code = waitForAuthCode();
  if (code.empty()) {
    outMsg = "Authentication timed out or failed. Did you close the browser?";
    return false;
  }

  return exchangeToken(code, verifier, outMsg);
}

namespace {

std::optional<int> fetchEpisodeCount(iHttpClient& http, int malId) {
  httpRequest request;
  request.url = "https://api.jikan.moe/v4/anime/" + std::to_string(malId);
  request.timeout = std::chrono::milliseconds(3000);
  auto response = http.execute(request);
  if (!response.ok() || response.value().body.empty())
    return std::nullopt;
  auto episodes = jsonLite::extractJsonNumber(response.value().body, "episodes");
  if (!episodes)
    return std::nullopt;
  return static_cast<int>(*episodes);
}

}  // namespace

size_t metadataSync::syncOngoing(database& db, iHttpClient& http,
                                 std::string& outMsg) {
  size_t updated = 0;

  for (auto e : db.getEntries(mediaType::kAnime)) {
    if (e.totalUnits != 0 || e.malId <= 0)
      continue;
    auto episodes = fetchEpisodeCount(http, e.malId);
    // Jikan rate-limits to roughly three requests per second.
    std::this_thread::sleep_for(std::chrono::milliseconds(340));
    if (!episodes || *episodes <= 0 || *episodes == e.totalUnits)
      continue;
    e.totalUnits = *episodes;
    db.updateEntry(e);
    updated++;
  }

  if (updated > 0) {
    db.saveAll();
    outMsg = "Reconciled " + std::to_string(updated) +
             " finished records from upstream MAL.";
  } else {
    outMsg = "All records synchronized with database.";
  }

  return updated;
}

}  // namespace catalist
