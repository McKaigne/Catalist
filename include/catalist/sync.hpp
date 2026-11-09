#ifndef CATALIST_SYNC_HPP_
#define CATALIST_SYNC_HPP_

#include <string>

#include "catalist/database.hpp"

namespace catalist {

struct malCredentials {
  std::string accessToken;
  std::string refreshToken;
  long long expiresAt = 0;
};

class malAuth {
 public:
  static malAuth& getInstance();

  bool isAuthenticated() const;
  bool authenticate(std::string& outMsg);
  void logout();

  std::string getAccessToken();
  void load();
  void save();

 private:
  malAuth();
  malCredentials creds_;
  bool exchangeToken(const std::string& code, const std::string& verifier,
                     std::string& outMsg);
};

class metadataSync {
 public:
  static size_t syncOngoing(database& db, std::string& outMsg);
};

}  // namespace catalist

#endif  // CATALIST_SYNC_HPP_
