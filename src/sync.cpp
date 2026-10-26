#include "catalist/sync.hpp"
#include "catalist/database.hpp"
#include <cstdlib>
#include <array>
#include <memory>
#include <regex>
#include <chrono>
#include <thread>
#include <random>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>

namespace catalist {

const std::string MAL_CLIENT_ID = "6114d00ca681b7701d1e15fe11a4987e"; // Standard open-source fallback client ID

struct PipeDeleter {
  void operator()(FILE* f) const {
    if (f) pclose(f);
  }
};

static std::string execCommand(const std::string& cmd) {
  std::array<char, 256> buffer;
  std::string result;
  std::unique_ptr<FILE, PipeDeleter> pipe(popen(cmd.c_str(), "r"));
  if (!pipe) return "";
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

static std::string extractJsonString(const std::string& block, const std::string& key) {
  std::string pattern = "\"" + key + "\":\"";
  size_t pos = block.find(pattern);
  if (pos == std::string::npos) {
    pattern = "\"" + key + "\": \"";
    pos = block.find(pattern);
    if (pos == std::string::npos) return "";
  }
  size_t start = pos + pattern.size();
  size_t end = start;
  while (end < block.size()) {
    if (block[end] == '"' && (end == 0 || block[end - 1] != '\\')) break;
    end++;
  }
  return (end > start) ? block.substr(start, end - start) : "";
}

static long long extractJsonNumber(const std::string& block, const std::string& key) {
  std::string pattern = "\"" + key + "\":";
  size_t pos = block.find(pattern);
  if (pos == std::string::npos) {
    pattern = "\"" + key + "\": ";
    pos = block.find(pattern);
    if (pos == std::string::npos) return 0;
  }
  size_t start = pos + pattern.size();
  while (start < block.size() && (block[start] == ' ' || block[start] == '\t')) start++;
  size_t end = start;
  while (end < block.size() && std::isdigit(block[end])) end++;
  if (end > start) {
    try { return std::stoll(block.substr(start, end - start)); } catch (...) {}
  }
  return 0;
}

// -----------------------------------------------------------------------------
// OAuth 2.0 PKCE & Loopback Server Implementation
// -----------------------------------------------------------------------------

MalAuth& MalAuth::getInstance() {
  static MalAuth instance;
  return instance;
}

MalAuth::MalAuth() {
  load();
}

bool MalAuth::isAuthenticated() const {
  return !creds_.accessToken.empty();
}

void MalAuth::logout() {
  creds_ = MalCredentials();
  save();
}

std::string MalAuth::getAccessToken() {
  // TODO: Add refresh token rotation check here before returning
  return creds_.accessToken;
}

void MalAuth::load() {
  auto path = Database::getConfigDir() / "mal_auth.json";
  std::ifstream file(path);
  if (!file.is_open()) return;
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  creds_.accessToken = extractJsonString(content, "access_token");
  creds_.refreshToken = extractJsonString(content, "refresh_token");
  creds_.expiresAt = extractJsonNumber(content, "expires_at");
}

void MalAuth::save() {
  auto path = Database::getConfigDir() / "mal_auth.json";
  std::ofstream file(path);
  if (file.is_open()) {
    file << "{\n"
         << "  \"access_token\": \"" << creds_.accessToken << "\",\n"
         << "  \"refresh_token\": \"" << creds_.refreshToken << "\",\n"
         << "  \"expires_at\": " << creds_.expiresAt << "\n"
         << "}\n";
    file.close();
    // Enforce 0600 permissions (S_IRUSR | S_IWUSR) for security
    chmod(path.c_str(), S_IRUSR | S_IWUSR);
  }
}

static std::string generateCodeVerifier() {
  const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
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
  if (server_fd == 0) return "";

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
  timeout.tv_sec = 60; // 60-second timeout to prevent permanently blocking thread
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
  if (read(new_socket, buffer, 2047) < 0) { /* ignore */ }
  std::string request(buffer);

  std::string code;
  std::regex codeRegex(R"(code=([^&\s]+))");
  std::smatch match;
  if (std::regex_search(request, match, codeRegex)) {
    code = match[1].str();
  }

  std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                         "<html><body style='font-family: sans-serif; text-align: center; margin-top: 10%;'>"
                         "<h2>Authentication Successful!</h2>"
                         "<p>You can securely close this tab and return to your terminal.</p>"
                         "</body></html>";
  if (write(new_socket, response.c_str(), response.length()) < 0) { /* ignore */ }

  close(new_socket);
  close(server_fd);

  return code;
}

bool MalAuth::exchangeToken(const std::string& code, const std::string& verifier, std::string& outMsg) {
  std::string cmd = "curl -s -X POST \"https://myanimelist.net/v1/oauth2/token\" "
                    "-H \"Content-Type: application/x-www-form-urlencoded\" "
                    "-d \"client_id=" + MAL_CLIENT_ID + "&code=" + code + "&code_verifier=" + verifier + "&grant_type=authorization_code\" 2>/dev/null";

  std::string json = execCommand(cmd);
  if (json.empty() || json.find("access_token") == std::string::npos) {
    outMsg = "Failed to exchange authorization code with MyAnimeList.";
    return false;
  }

  creds_.accessToken = extractJsonString(json, "access_token");
  creds_.refreshToken = extractJsonString(json, "refresh_token");
  long long expiresIn = extractJsonNumber(json, "expires_in");

  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
  creds_.expiresAt = seconds + expiresIn;

  save();
  outMsg = "Successfully authenticated with MyAnimeList.";
  return true;
}

bool MalAuth::authenticate(std::string& outMsg) {
  std::string verifier = generateCodeVerifier();
  std::string authUrl = "https://myanimelist.net/v1/oauth2/authorize?response_type=code&client_id=" + MAL_CLIENT_ID + "&code_challenge=" + verifier + "&code_challenge_method=plain";

  std::string openCmd;
#if defined(_WIN32)
  openCmd = "start \"\" \"" + authUrl + "\"";
#elif defined(__APPLE__)
  openCmd = "open \"" + authUrl + "\"";
#else
  openCmd = "xdg-open \"" + authUrl + "\" >/dev/null 2>&1 &";
#endif

  int ret = std::system(openCmd.c_str());
  (void)ret;

  std::string code = waitForAuthCode();
  if (code.empty()) {
    outMsg = "Authentication timed out or failed. Did you close the browser?";
    return false;
  }

  return exchangeToken(code, verifier, outMsg);
}

size_t MetadataSync::syncOngoing(Database& db, std::string& outMsg) {
  size_t updated = 0;

  for (auto e : db.getEntries(MediaType::Anime)) {
    if (e.totalUnits == 0 && e.malId > 0) {
      std::string cmd = "curl -s -m 3 \"https://api.jikan.moe/v4/anime/" + std::to_string(e.malId) + "\" 2>/dev/null";
      std::string resp = execCommand(cmd);
      std::this_thread::sleep_for(std::chrono::milliseconds(340));
      if (resp.empty()) continue;

      std::regex epRe(R"("episodes":\s*(\d+))");
      std::smatch m;
      if (std::regex_search(resp, m, epRe)) {
        try {
          int eps = std::stoi(m[1].str());
          if (eps > 0 && eps != e.totalUnits) {
            e.totalUnits = eps;
            db.updateEntry(e);
            updated++;
          }
        } catch (...) {}
      }
    }
  }

  if (updated > 0) {
    db.saveAll();
    outMsg = "Reconciled " + std::to_string(updated) + " finished records from upstream MAL.";
  } else {
    outMsg = "All records synchronized with database.";
  }

  return updated;
}

} // namespace catalist
