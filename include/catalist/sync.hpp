#pragma once

#include "catalist/database.hpp"
#include <string>

namespace catalist {

struct MalCredentials {
  std::string accessToken;
  std::string refreshToken;
  long long expiresAt = 0;
};

class MalAuth {
public:
  static MalAuth& getInstance();

  bool isAuthenticated() const;
  bool authenticate(std::string& outMsg);
  void logout();
  
  std::string getAccessToken();
  void load();
  void save();

private:
  MalAuth();
  MalCredentials creds_;
  bool exchangeToken(const std::string& code, const std::string& verifier, std::string& outMsg);
};

class MetadataSync {
public:
  static size_t syncOngoing(Database& db, std::string& outMsg);
};

} // namespace catalist
