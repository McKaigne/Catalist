#ifndef CATALIST_NET_LIST_SYNC_HPP_
#define CATALIST_NET_LIST_SYNC_HPP_

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "catalist/core/clock.hpp"
#include "catalist/net/listService.hpp"
#include "catalist/storage/database.hpp"

namespace catalist {

struct syncReport {
  int added = 0;
  int updated = 0;
  int pushed = 0;
  int removedLocally = 0;
  int removedRemotely = 0;
  std::vector<std::string> problems;

  [[nodiscard]] bool ok() const { return problems.empty(); }
  // One line for a status bar or a popup.
  [[nodiscard]] std::string summary() const;
};

// Two-way sync of the local catalogue with every connected list service:
//  - remote rows not yet local are added; entries edited locally (dirty) are
//    pushed; other matched entries take the remote values;
//  - on first contact with a service the newer side wins, and a tie pushes;
//  - local deletions of synced entries are deleted remotely; an entry that
//    was synced with a service but is gone from it is deleted locally;
//  - a backup snapshot is taken first, and the catalogue is saved after.
class listSync {
public:
  // `mapper` (may be null) fills in missing MAL/AniList ids so one service's
  // entries can reach the other.
  listSync(database& db, const iClock& clock,
           std::vector<iListService*> services, iIdMapper* mapper);

  [[nodiscard]] syncReport run();

  // One-way import of a list (the logged-in user's, or a public one by name):
  // new rows are added; existing entries only gain ids, totals and the user
  // fields they lack. Nothing is pushed or deleted.
  [[nodiscard]] syncReport importFrom(iListService& service, mediaType type,
                                      const std::optional<std::string>& userName);

private:
  void fillMissingIds(mediaType type, syncReport& report,
                      const std::vector<iListService*>& connected);
  // Returns the number of entries added locally.
  int syncList(iListService& service, mediaType type, syncReport& report,
               const isoDate& today,
               std::set<std::pair<mediaType, int>>& unpushed, bool& fetchFailed);
  void clearPushedDirtyFlags(const std::set<std::pair<mediaType, int>>& unpushed);

  database& db_;
  const iClock& clock_;
  std::vector<iListService*> services_;
  iIdMapper* mapper_;
};

}  // namespace catalist

#endif  // CATALIST_NET_LIST_SYNC_HPP_
