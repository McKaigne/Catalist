#ifndef CATALIST_NET_ANILIST_LIST_SERVICE_HPP_
#define CATALIST_NET_ANILIST_LIST_SERVICE_HPP_

#include <functional>

#include "catalist/core/json.hpp"
#include "catalist/net/anilistAuth.hpp"
#include "catalist/net/listService.hpp"
#include "catalist/platform/httpClient.hpp"

namespace catalist {

struct anilistViewer {
  int id = 0;
  std::string name;
};

// The AniList GraphQL API. `pace` runs before every request to stay under
// the per-minute rate limit.
class anilistListService : public iListService, public iIdMapper {
public:
  anilistListService(iHttpClient& http, anilistAuth& auth,
                     std::function<void()> pace);

  [[nodiscard]] std::string key() const override { return "anilist"; }
  [[nodiscard]] std::string displayName() const override { return "AniList"; }
  [[nodiscard]] bool isConnected() const override;

  [[nodiscard]] result<std::vector<remoteListEntry>> fetchList(
      mediaType type, const std::optional<std::string>& userName) override;
  [[nodiscard]] syncOutcome saveEntry(const entry& item) override;
  [[nodiscard]] syncOutcome deleteEntry(const remoteListEntry& row) override;
  [[nodiscard]] std::optional<int> idOf(const entry& item) const override;

  [[nodiscard]] result<std::map<int, int>> anilistIdsForMal(
      mediaType type, const std::vector<int>& malIds) override;
  [[nodiscard]] result<std::map<int, int>> malIdsForAnilist(
      mediaType type, const std::vector<int>& anilistIds) override;

  // The logged-in user; also how a pasted token is checked.
  [[nodiscard]] result<anilistViewer> viewer();

private:
  // The `data` member of the reply, or the first GraphQL error.
  [[nodiscard]] result<json::value> query(const std::string& document,
                                          json::value variables);
  [[nodiscard]] result<std::map<int, int>> lookup(mediaType type,
                                                  const std::vector<int>& ids,
                                                  bool idsAreMal);

  iHttpClient& http_;
  anilistAuth& auth_;
  std::function<void()> pace_;
};

}  // namespace catalist

#endif  // CATALIST_NET_ANILIST_LIST_SERVICE_HPP_
