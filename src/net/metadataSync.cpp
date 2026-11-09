#include "catalist/net/metadataSync.hpp"

#include <chrono>
#include <optional>
#include <thread>

#include "catalist/core/jsonLite.hpp"

namespace catalist {

namespace {

std::optional<int> fetchEpisodeCount(iHttpClient& http, int malId) {
  httpRequest request;
  request.url = "https://api.jikan.moe/v4/anime/" + std::to_string(malId);
  request.timeout = std::chrono::milliseconds(3000);
  auto response = http.execute(request);
  if (!response.ok() || response.value().body.empty())
    return std::nullopt;
  auto episodes = jsonLite::extractJsonNumber(response.value().body, "episodes");
  if (!episodes)
    return std::nullopt;
  return static_cast<int>(*episodes);
}

}  // namespace

size_t metadataSync::syncOngoing(database& db, iHttpClient& http,
                                 std::string& outMsg) {
  size_t updated = 0;

  for (auto e : db.getEntries(mediaType::kAnime)) {
    if (e.totalUnits != 0 || e.malId <= 0)
      continue;
    auto episodes = fetchEpisodeCount(http, e.malId);
    // Jikan rate-limits to roughly three requests per second.
    std::this_thread::sleep_for(std::chrono::milliseconds(340));
    if (!episodes || *episodes <= 0 || *episodes == e.totalUnits)
      continue;
    e.totalUnits = *episodes;
    db.updateEntry(e);
    updated++;
  }

  if (updated > 0) {
    db.saveAll();
    outMsg = "Reconciled " + std::to_string(updated) +
             " finished records from upstream MAL.";
  } else {
    outMsg = "All records synchronized with database.";
  }

  return updated;
}

}  // namespace catalist
