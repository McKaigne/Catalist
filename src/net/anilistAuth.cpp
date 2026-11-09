#include "catalist/net/anilistAuth.hpp"

#include "catalist/core/json.hpp"
#include "catalist/core/url.hpp"

namespace catalist {

namespace {

std::string trimmed(const std::string& text) {
  size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  size_t last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

}  // namespace

anilistAuth::anilistAuth(iFileSystem& fileSystem,
                         std::filesystem::path tokenPath, std::string clientId)
    : fileSystem_(fileSystem),
      tokenPath_(std::move(tokenPath)),
      clientId_(std::move(clientId)) {
  load();
}

std::optional<std::string> anilistAuth::authorizeUrl() const {
  if (clientId_.empty())
    return std::nullopt;
  return "https://anilist.co/api/v2/oauth/authorize?client_id=" +
         url::encode(clientId_) + "&response_type=token";
}

bool anilistAuth::isAuthenticated() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !token_.empty();
}

std::string anilistAuth::accessToken() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return token_;
}

bool anilistAuth::storeToken(const std::string& token) {
  std::string cleaned = trimmed(token);
  if (cleaned.empty())
    return false;
  json::value document;
  document.set("access_token", cleaned);
  if (!fileSystem_.writeFileAtomic(tokenPath_, json::serialize(document, true) + "\n"))
    return false;
  // Tokens grant account access, so the file must not be world-readable.
  fileSystem_.restrictToOwner(tokenPath_);
  std::lock_guard<std::mutex> lock(mutex_);
  token_ = cleaned;
  return true;
}

void anilistAuth::logout() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    token_.clear();
  }
  fileSystem_.removeFile(tokenPath_);
}

void anilistAuth::load() {
  auto content = fileSystem_.readFile(tokenPath_);
  if (!content)
    return;
  auto parsed = json::parse(*content);
  if (!parsed.ok())
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  token_ = parsed.value()["access_token"].asString().value_or("");
}

}  // namespace catalist
