#ifndef CATALIST_NET_ACCOUNTS_HPP_
#define CATALIST_NET_ACCOUNTS_HPP_

#include <optional>
#include <string>
#include <vector>

#include "catalist/net/anilistAuth.hpp"
#include "catalist/net/anilistListService.hpp"
#include "catalist/net/malAuth.hpp"
#include "catalist/net/malListService.hpp"

namespace catalist {

// The MyAnimeList and AniList logins and list services, built once by
// main.cpp (or a test) and shared by the CLI and the TUI.
struct listAccounts {
  malAuth& malLogin;
  anilistAuth& anilistLogin;
  malListService& mal;
  anilistListService& anilist;

  [[nodiscard]] std::vector<iListService*> services() const {
    return {&mal, &anilist};
  }
  // "mal" or "anilist" (also "myanimelist"); absent for anything else.
  [[nodiscard]] iListService* find(const std::string& name) const {
    if (name == "mal" || name == "myanimelist")
      return &mal;
    if (name == "anilist")
      return &anilist;
    return nullptr;
  }
};

}  // namespace catalist

#endif  // CATALIST_NET_ACCOUNTS_HPP_
