#ifndef CATALIST_SYNC_HPP_
#define CATALIST_SYNC_HPP_

#include <filesystem>
#include <string>

#include "catalist/core/clock.hpp"
#include "catalist/database.hpp"
#include "catalist/platform/fileSystem.hpp"
#include "catalist/platform/httpClient.hpp"
#include "catalist/platform/processLauncher.hpp"

namespace catalist {

struct malCredentials {
  std::string accessToken;
  std::string refreshToken;
  long long expiresAt = 0;
};

class malAuth {
 public:
  malAuth(iFileSystem& fileSystem, iHttpClient& http,
          iProcessLauncher& launcher, const iClock& clock,
          std::filesystem::path credentialsPath);

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
  const iClock& clock_;
  std::filesystem::path credentialsPath_;
  malCredentials creds_;
};

class metadataSync {
 public:
  static size_t syncOngoing(database& db, iHttpClient& http,
                            std::string& outMsg);
};

}  // namespace catalist

#endif  // CATALIST_SYNC_HPP_
