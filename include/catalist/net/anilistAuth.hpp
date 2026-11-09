#ifndef CATALIST_NET_ANILIST_AUTH_HPP_
#define CATALIST_NET_ANILIST_AUTH_HPP_

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

#include "catalist/platform/fileSystem.hpp"

namespace catalist {

// AniList's implicit grant hands the token back in a URL fragment, which a
// local server cannot receive, so login opens the authorize page with the
// "PIN" redirect and the user pastes the token it shows. Tokens last a year.
class anilistAuth {
public:
  // clientId may be empty, in which case login is unavailable.
  anilistAuth(iFileSystem& fileSystem, std::filesystem::path tokenPath,
              std::string clientId);

  [[nodiscard]] bool hasClientId() const { return !clientId_.empty(); }
  [[nodiscard]] std::optional<std::string> authorizeUrl() const;

  [[nodiscard]] bool isAuthenticated() const;
  [[nodiscard]] std::string accessToken() const;
  // Stores a pasted token (surrounding whitespace removed); false when empty
  // or unwritable.
  bool storeToken(const std::string& token);
  void logout();

private:
  void load();

  iFileSystem& fileSystem_;
  std::filesystem::path tokenPath_;
  std::string clientId_;
  // Guards token_: login and sync run on a worker thread while the UI reads
  // the login state.
  mutable std::mutex mutex_;
  std::string token_;
};

}  // namespace catalist

#endif  // CATALIST_NET_ANILIST_AUTH_HPP_
