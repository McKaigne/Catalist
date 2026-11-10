#include "catalist/model/serviceLinks.hpp"

namespace catalist {

namespace {

struct serviceLink {
  const char* badge;
  const char* site;
  int entry::*id;
};

constexpr serviceLink kServiceLinks[] = {
    {"MAL", "https://myanimelist.net/", &entry::malId},
    {"AL", "https://anilist.co/", &entry::anilistId},
};

}  // namespace

std::string webAddress(const entry& mediaEntry) {
  std::string kind = mediaEntry.type == mediaType::kAnime ? "anime" : "manga";
  for (const auto& link : kServiceLinks) {
    int id = mediaEntry.*link.id;
    if (id > 0)
      return link.site + kind + "/" + std::to_string(id);
  }
  return "";
}

std::vector<std::string> serviceBadges(const entry& mediaEntry) {
  std::vector<std::string> badges;
  for (const auto& link : kServiceLinks) {
    int id = mediaEntry.*link.id;
    if (id > 0)
      badges.push_back(std::string(link.badge) + " #" + std::to_string(id));
  }
  return badges;
}

}  // namespace catalist
