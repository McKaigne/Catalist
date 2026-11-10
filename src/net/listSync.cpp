#include "catalist/net/listSync.hpp"

#include <map>
#include <set>

#include "catalist/core/logger.hpp"
#include "catalist/model/entryOps.hpp"

namespace catalist {

namespace {

using remoteRows = std::map<int, remoteListEntry>;

bool sameListFields(const entry& a, const entry& b) {
  return a.status == b.status && a.score == b.score &&
         a.progress == b.progress && a.volumes == b.volumes &&
         a.rewatched == b.rewatched && a.startDate == b.startDate &&
         a.finishDate == b.finishDate && a.notes == b.notes &&
         a.totalUnits == b.totalUnits && a.totalVolumes == b.totalVolumes &&
         a.malId == b.malId && a.anilistId == b.anilistId;
}

// The remote row's list values replace the local ones; the local title and
// any id the remote lacks are kept.
entry withRemoteValues(entry local, const entry& remote, const isoDate& today) {
  entry merged = local;
  merged.status = remote.status;
  merged.score = remote.score;
  merged.progress = remote.progress;
  merged.volumes = remote.volumes;
  merged.rewatched = remote.rewatched;
  merged.startDate = remote.startDate;
  merged.finishDate = remote.finishDate;
  merged.notes = remote.notes;
  if (remote.totalUnits > 0)
    merged.totalUnits = remote.totalUnits;
  if (remote.totalVolumes > 0)
    merged.totalVolumes = remote.totalVolumes;
  adoptServiceIds(merged, remote);
  if (!sameListFields(merged, local))
    merged.updatedAt = today;
  return merged;
}

// On first contact the side changed more recently wins; a tie (or no
// remote date) keeps the local values, which only rewrites them remotely.
bool localIsNewer(const entry& local, const entry& remote) {
  if (!local.updatedAt)
    return !remote.updatedAt;
  return !remote.updatedAt || *local.updatedAt >= *remote.updatedAt;
}

int deletionIdFor(const iListService& service, const deletedEntry& deleted) {
  return service.key() == "mal" ? deleted.malId : deleted.anilistId;
}

std::optional<entry> findByAnyId(const database& db, const entry& row) {
  for (const auto& local : db.getEntries(row.type)) {
    bool sameMal = row.malId > 0 && local.malId == row.malId;
    bool sameAnilist = row.anilistId > 0 && local.anilistId == row.anilistId;
    if (sameMal || sameAnilist)
      return local;
  }
  return std::nullopt;
}

entry newLocalEntry(const entry& row) {
  entry fresh = row;
  fresh.id = 0;
  fresh.dirty = false;
  fresh.syncedWith.clear();
  return fresh;
}

std::string serviceProblem(const iListService& service, mediaType type,
                           const std::string& message) {
  return service.displayName() + " (" + typeToString(type) + "): " + message;
}

class syncPass {
public:
  syncPass(database& db, const isoDate& today, iListService& service,
           mediaType type, syncReport& report,
           std::set<std::pair<mediaType, int>>& unpushed)
      : db_(db),
        today_(today),
        service_(service),
        type_(type),
        report_(report),
        unpushed_(unpushed) {}

  // Returns false when the remote list could not be read.
  bool fetch() {
    auto fetched = service_.fetchList(type_, std::nullopt);
    if (!fetched.ok()) {
      report_.problems.push_back(
          serviceProblem(service_, type_, fetched.errorMessage()));
      return false;
    }
    for (auto& row : fetched.value()) {
      if (auto id = service_.idOf(row.item))
        remote_[*id] = std::move(row);
    }
    return true;
  }

  void pushDeletions() {
    for (const auto& deleted : db_.pendingDeletions()) {
      if (deleted.type == type_ && deleted.pendingServices.count(service_.key()))
        pushDeletion(deleted);
    }
  }

  void reconcileLocalEntries() {
    std::vector<entry> locals = db_.getEntries(type_);
    for (auto& local : locals)
      reconcile(local);
  }

  // Returns the number of entries added locally.
  int addRemoteOnlyRows() {
    int added = 0;
    remoteRows leftover = remote_;
    for (const auto& [id, row] : leftover) {
      if (auto existing = findByAnyId(db_, row.item)) {
        linkAndReconcile(*existing, row.item);
        continue;
      }
      entry fresh = newLocalEntry(row.item);
      fresh.syncedWith.insert(service_.key());
      db_.addEntry(fresh);
      ++added;
    }
    report_.added += added;
    return added;
  }

private:
  void pushDeletion(const deletedEntry& deleted) {
    int id = deletionIdFor(service_, deleted);
    auto found = remote_.find(id);
    if (id <= 0 || found == remote_.end()) {
      db_.settleDeletion(deleted, service_.key());
      return;
    }
    auto outcome = service_.deleteEntry(found->second);
    // Either way the row must not come back as "new" in this pass.
    remote_.erase(found);
    if (!outcome.ok()) {
      report_.problems.push_back(
          serviceProblem(service_, type_, outcome.errorMessage()));
      return;
    }
    db_.settleDeletion(deleted, service_.key());
    ++report_.removedRemotely;
  }

  void push(entry& local) {
    auto outcome = service_.saveEntry(local);
    if (!outcome.ok()) {
      report_.problems.push_back(
          serviceProblem(service_, type_, outcome.errorMessage()));
      unpushed_.insert({local.type, local.id});
      return;
    }
    local.syncedWith.insert(service_.key());
    ++report_.pushed;
  }

  void reconcileMissingRemotely(entry& local) {
    bool deletedThere = local.syncedWith.count(service_.key()) && !local.dirty;
    if (!deletedThere) {
      push(local);
      db_.updateEntry(local);
      return;
    }
    LOG_INFO("Removed '" + local.title + "': deleted on " +
             service_.displayName());
    db_.removeEntry(local.type, local.id);
    db_.settleDeletion({local.type, local.malId, local.anilistId, local.title, {}},
                       service_.key());
    ++report_.removedLocally;
  }

  void reconcile(entry& local) {
    auto id = service_.idOf(local);
    if (!id)
      return;
    auto found = remote_.find(*id);
    if (found == remote_.end()) {
      reconcileMissingRemotely(local);
      return;
    }
    const entry& row = found->second.item;
    bool firstContact = !local.syncedWith.count(service_.key());
    if (local.dirty || (firstContact && localIsNewer(local, row))) {
      push(local);
    } else {
      entry merged = withRemoteValues(local, row, today_);
      if (!sameListFields(merged, local))
        ++report_.updated;
      local = merged;
    }
    local.syncedWith.insert(service_.key());
    db_.updateEntry(local);
    remote_.erase(found);
  }

  void linkAndReconcile(entry local, const entry& row) {
    adoptServiceIds(local, row);
    reconcile(local);
  }

  database& db_;
  const isoDate& today_;
  iListService& service_;
  mediaType type_;
  syncReport& report_;
  std::set<std::pair<mediaType, int>>& unpushed_;
  remoteRows remote_;
};

std::vector<int> idsMissing(const std::vector<entry>& entries, bool needAnilist) {
  std::vector<int> ids;
  for (const auto& item : entries) {
    if (needAnilist && item.malId > 0 && item.anilistId <= 0)
      ids.push_back(item.malId);
    if (!needAnilist && item.anilistId > 0 && item.malId <= 0)
      ids.push_back(item.anilistId);
  }
  return ids;
}

}  // namespace

std::string syncReport::summary() const {
  std::string text = std::to_string(added) + " added, " +
                     std::to_string(updated) + " updated, " +
                     std::to_string(pushed) + " sent, " +
                     std::to_string(removedLocally) + " removed here, " +
                     std::to_string(removedRemotely) + " removed there.";
  if (problems.empty())
    return text;
  return text + " " + std::to_string(problems.size()) +
         (problems.size() == 1 ? " problem: " : " problems, first: ") +
         problems.front();
}

listSync::listSync(database& db, const iClock& clock,
                   std::vector<iListService*> services, iIdMapper* mapper)
    : db_(db), clock_(clock), services_(std::move(services)), mapper_(mapper) {}

void listSync::fillMissingIds(mediaType type, syncReport& report,
                              const std::vector<iListService*>& connected) {
  if (!mapper_)
    return;
  bool wantAnilist = false;
  bool wantMal = false;
  for (auto* service : connected) {
    wantAnilist = wantAnilist || service->key() == "anilist";
    wantMal = wantMal || service->key() == "mal";
  }
  const auto& entries = db_.getEntries(type);
  auto needingAnilist = wantAnilist ? idsMissing(entries, true) : std::vector<int>{};
  auto needingMal = wantMal ? idsMissing(entries, false) : std::vector<int>{};
  std::map<int, int> toAnilist;
  std::map<int, int> toMal;
  if (!needingAnilist.empty()) {
    auto mapped = mapper_->anilistIdsForMal(type, needingAnilist);
    if (mapped.ok())
      toAnilist = mapped.value();
    else
      report.problems.push_back("Looking up AniList ids: " + mapped.errorMessage());
  }
  if (!needingMal.empty()) {
    auto mapped = mapper_->malIdsForAnilist(type, needingMal);
    if (mapped.ok())
      toMal = mapped.value();
    else
      report.problems.push_back("Looking up MyAnimeList ids: " + mapped.errorMessage());
  }
  std::vector<entry> updates;
  for (entry item : db_.getEntries(type)) {
    auto anilist = toAnilist.find(item.malId);
    auto mal = toMal.find(item.anilistId);
    if (item.anilistId <= 0 && anilist != toAnilist.end())
      item.anilistId = anilist->second;
    else if (item.malId <= 0 && mal != toMal.end())
      item.malId = mal->second;
    else
      continue;
    updates.push_back(item);
  }
  for (const auto& item : updates)
    db_.updateEntry(item);
}

syncReport listSync::run() {
  syncReport report;
  std::vector<iListService*> connected;
  for (auto* service : services_) {
    if (service->isConnected())
      connected.push_back(service);
  }
  if (connected.empty()) {
    report.problems.push_back("Log in to MyAnimeList or AniList first.");
    return report;
  }
  db_.createBackupSnapshot();

  isoDate today = localDate(clock_);
  std::set<std::pair<mediaType, int>> unpushed;
  bool fetchFailed = false;
  // A second round carries rows that the later service added over to the
  // earlier one in the same run.
  for (int round = 0; round < 2; ++round) {
    for (auto type : {mediaType::kAnime, mediaType::kManga})
      fillMissingIds(type, report, connected);
    int added = 0;
    for (auto* service : connected) {
      for (auto type : {mediaType::kAnime, mediaType::kManga})
        added += syncList(*service, type, report, today, unpushed, fetchFailed);
    }
    if (added == 0 || connected.size() < 2)
      break;
  }

  if (!fetchFailed)
    clearPushedDirtyFlags(unpushed);
  db_.saveAll();
  return report;
}

int listSync::syncList(iListService& service, mediaType type,
                       syncReport& report, const isoDate& today,
                       std::set<std::pair<mediaType, int>>& unpushed,
                       bool& fetchFailed) {
  syncPass pass(db_, today, service, type, report, unpushed);
  if (!pass.fetch()) {
    fetchFailed = true;
    return 0;
  }
  pass.pushDeletions();
  pass.reconcileLocalEntries();
  return pass.addRemoteOnlyRows();
}

void listSync::clearPushedDirtyFlags(
    const std::set<std::pair<mediaType, int>>& unpushed) {
  for (auto type : {mediaType::kAnime, mediaType::kManga}) {
    std::vector<entry> entries = db_.getEntries(type);
    for (auto& item : entries) {
      if (!item.dirty || unpushed.count({type, item.id}))
        continue;
      item.dirty = false;
      db_.updateEntry(item);
    }
  }
}

syncReport listSync::importFrom(iListService& service, mediaType type,
                                const std::optional<std::string>& userName) {
  syncReport report;
  auto fetched = service.fetchList(type, userName);
  if (!fetched.ok()) {
    report.problems.push_back(serviceProblem(service, type, fetched.errorMessage()));
    return report;
  }
  db_.createBackupSnapshot();
  for (const auto& row : fetched.value()) {
    auto existing = findByAnyId(db_, row.item);
    if (!existing) {
      db_.addEntry(newLocalEntry(row.item));
      ++report.added;
      continue;
    }
    entry merged = mergeImported(*existing, row.item);
    if (sameListFields(merged, *existing))
      continue;
    db_.updateEntry(merged);
    ++report.updated;
  }
  db_.saveAll();
  return report;
}

}  // namespace catalist
