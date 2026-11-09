#include "catalist/sync.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <optional>
#include <random>
#include <sstream>
#include <thread>

#include "catalist/core/jsonLite.hpp"
#include "catalist/core/url.hpp"
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
                 iProcessLauncher& launcher,
                 iAuthCallbackServer& callbackServer, const iClock& clock,
                 std::filesystem::path credentialsPath)
    : fileSystem_(fileSystem),
      http_(http),
      launcher_(launcher),
      callbackServer_(callbackServer),
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

namespace {

constexpr auto kAuthTimeout = std::chrono::seconds(60);

class socketHandle {
 public:
  explicit socketHandle(int descriptor) : descriptor_(descriptor) {}
  ~socketHandle() {
    if (descriptor_ >= 0)
      close(descriptor_);
  }
  socketHandle(const socketHandle&) = delete;
  socketHandle& operator=(const socketHandle&) = delete;

  [[nodiscard]] int get() const { return descriptor_; }
  [[nodiscard]] bool valid() const { return descriptor_ >= 0; }

 private:
  int descriptor_;
};

bool bindLoopback(int descriptor, std::uint16_t port) {
  int reuse = 1;
  setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  auto* generic = reinterpret_cast<sockaddr*>(&address);
  return bind(descriptor, generic, sizeof(address)) == 0 &&
         listen(descriptor, 1) == 0;
}

bool waitReadable(int descriptor, std::chrono::seconds timeout) {
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(descriptor, &readable);
  timeval limit{};
  limit.tv_sec = static_cast<time_t>(timeout.count());
  return select(descriptor + 1, &readable, nullptr, nullptr, &limit) > 0 &&
         FD_ISSET(descriptor, &readable);
}

std::string readRequest(int descriptor) {
  std::array<char, 2048> buffer{};
  ssize_t received = read(descriptor, buffer.data(), buffer.size() - 1);
  if (received <= 0)
    return "";
  return std::string(buffer.data(), static_cast<size_t>(received));
}

void sendSuccessPage(int descriptor) {
  const std::string page =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
      "<html><body style='font-family: sans-serif; text-align: center; "
      "margin-top: 10%;'>"
      "<h2>Authentication Successful!</h2>"
      "<p>You can securely close this tab and return to your terminal.</p>"
      "</body></html>";
  // The browser tab is cosmetic; a failed write does not affect the login.
  [[maybe_unused]] ssize_t written = write(descriptor, page.data(), page.size());
}

std::string requestTarget(const std::string& httpRequestText) {
  size_t lineEnd = httpRequestText.find_first_of("\r\n");
  std::string requestLine = httpRequestText.substr(0, lineEnd);
  size_t targetStart = requestLine.find(' ');
  if (targetStart == std::string::npos)
    return "";
  size_t targetEnd = requestLine.find(' ', targetStart + 1);
  return requestLine.substr(targetStart + 1, targetEnd - targetStart - 1);
}

}  // namespace

std::optional<std::string> parseAuthCode(const std::string& httpRequestText) {
  std::string target = requestTarget(httpRequestText);
  size_t queryStart = target.find('?');
  if (queryStart == std::string::npos)
    return std::nullopt;

  std::stringstream query(target.substr(queryStart + 1));
  std::string parameter;
  while (std::getline(query, parameter, '&')) {
    if (parameter.rfind("code=", 0) != 0)
      continue;
    auto code = url::decode(parameter.substr(5));
    if (!code || code->empty())
      return std::nullopt;
    return code;
  }
  return std::nullopt;
}

std::optional<std::string> loopbackAuthServer::waitForCode(
    std::chrono::seconds timeout) {
  socketHandle server(socket(AF_INET, SOCK_STREAM, 0));
  if (!server.valid() || !bindLoopback(server.get(), port_))
    return std::nullopt;
  if (!waitReadable(server.get(), timeout))
    return std::nullopt;

  socketHandle client(accept(server.get(), nullptr, nullptr));
  if (!client.valid())
    return std::nullopt;

  auto code = parseAuthCode(readRequest(client.get()));
  sendSuccessPage(client.get());
  return code;
}

bool malAuth::exchangeToken(const std::string& code,
                            const std::string& verifier, std::string& outMsg) {
  httpRequest request;
  request.url = "https://myanimelist.net/v1/oauth2/token";
  request.method = "POST";
  request.headers["Content-Type"] = "application/x-www-form-urlencoded";
  request.body = "client_id=" + url::encode(kMalClientId) +
                 "&code=" + url::encode(code) +
                 "&code_verifier=" + url::encode(verifier) +
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

  auto code = callbackServer_.waitForCode(kAuthTimeout);
  if (!code) {
    outMsg = "Authentication timed out or failed. Did you close the browser?";
    return false;
  }

  return exchangeToken(*code, verifier, outMsg);
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
