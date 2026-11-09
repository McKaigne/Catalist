#ifndef CATALIST_NET_MAL_LIST_SERVICE_HPP_
#define CATALIST_NET_MAL_LIST_SERVICE_HPP_

#include <functional>

#include "catalist/net/listService.hpp"
#include "catalist/net/malAuth.hpp"
#include "catalist/platform/httpClient.hpp"

namespace catalist {

// The MyAnimeList v2 API: /users/{name}/animelist and the my_list_status
// endpoints. `pace` runs before every request to respect the rate limit.
class malListService : public iListService {
public:
  malListService(iHttpClient& http, malAuth& auth, std::function<void()> pace);

  [[nodiscard]] std::string key() const override { return "mal"; }
  [[nodiscard]] std::string displayName() const override {
    return "MyAnimeList";
  }
  [[nodiscard]] bool isConnected() const override;

  [[nodiscard]] result<std::vector<remoteListEntry>> fetchList(
      mediaType type, const std::optional<std::string>& userName) override;
  [[nodiscard]] syncOutcome saveEntry(const entry& item) override;
  [[nodiscard]] syncOutcome deleteEntry(const remoteListEntry& row) override;
  [[nodiscard]] std::optional<int> idOf(const entry& item) const override;

private:
  [[nodiscard]] result<httpResponse> send(httpRequest request,
                                          bool needsLogin);

  iHttpClient& http_;
  malAuth& auth_;
  std::function<void()> pace_;
};

}  // namespace catalist

#endif  // CATALIST_NET_MAL_LIST_SERVICE_HPP_
