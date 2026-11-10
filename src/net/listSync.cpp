#include "catalist/net/listSync.hpp"

#include <map>
#include <set>

#include "catalist/core/logger.hpp"
#include "catalist/model/entryOps.hpp"

namespace catalist {

namespace {

bool sameListFields(const entry& a, const entry& b) {
  return a.status == b.status && a.score == b.score &&
         a.progress == b.progress && a.volumes == b.volumes &&
         a.rewatched == b.rewatched && a.startDate == b.startDate &&
         a.finishDate == b.finishDate && a.notes == b.notes;
}

// The list fields plus what the service owns: totals and ids.
bool sameDetails(const entry& a, const entry& b) {
  return sameListFields(a, b) && a.totalUnits == b.totalUnits &&
         a.totalVolumes == b.totalVolumes && a.malId == b.malId &&
         a.anilistId == b.anilistId;
}

void copyListFields(entry& target, const entry& source) {
  target.status = source.status;
  target.score = source.score;
  target.progress = source.progress;
  target.volumes = source.volumes;
  target.rewatched = source.rewatched;
  target.startDate = source.startDate;
  target.finishDate = source.finishDate;
  target.notes = source.notes;
}

// Totals and ids come from the service; the local title is kept.
void takeServiceDetails(entry& local, const entry& remote) {
  if (remote.totalUnits > 0)
    local.totalUnits = remote.totalUnits;
  if (remote.totalVolumes > 0)
    local.totalVolumes = remote.totalVolumes;
  adoptServiceIds(local, remote);
}

// The side that moved away from the base wins; when both did and disagree,
// the local value stays and the field is named in `clashes`.
template <typename value>
void mergeField(entry& merged, const entry& base, const entry& remote,
                value entry::*field, const char* name,
                std::vector<std::string>& clashes) {
  value& local = merged.*field;
  if (local == base.*field) {
    local = remote.*field;
    return;
  }
  if (remote.*field != base.*field && remote.*field != local)
    clashes.push_back(name);
}

entry mergeWithBase(const entry& local, const entry& base, const entry& remote,
                    std::vector<std::string>& clashes) {
  entry merged = local;
  mergeField(merged, base, remote, &entry::status, "status", clashes);
  mergeField(merged, base, remote, &entry::score, "score", clashes);
  mergeField(merged, base, remote, &entry::progress, "progress", clashes);
  mergeField(merged, base, remote, &entry::volumes, "volumes", clashes);
  mergeField(merged, base, remote, &entry::rewatched, "rewatches", clashes);
  mergeField(merged, base, remote, &entry::startDate, "start date", clashes);
  mergeField(merged, base, remote, &entry::finishDate, "finish date", clashes);
  mergeField(merged, base, remote, &entry::notes, "notes", clashes);
  return merged;
}

// On first contact the side changed more recently wins; a tie (or no
// remote date) keeps the local values.
bool localIsNewer(const entry& local, const entry& remote) {
  if (!local.updatedAt)
    return !remote.updatedAt;
  return !remote.updatedAt || *local.updatedAt >= *remote.updatedAt;
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
  return fresh;
}

std::string serviceProblem(const iListService& service, mediaType type,
                           const std::string& message) {
  return service.displayName() + " (" + typeToString(type) + "): " + message;
}

std::string joined(const std::vector<std::string>& words) {
  std::string text;
  for (const auto& word : words)
    text += (text.empty() ? "" : ", ") + word;
  return text;
}

template <typename value>
const value* findId(const std::map<int, value>& byId, int id) {
  auto found = byId.find(id);
  return found == byId.end() ? nullptr : &found->second;
}

// One service and one media type: every id seen locally, in the bases or in
// the fetched list is settled by the table in ADR-012.
class syncPass {
public:
  syncPass(database& db, const isoDate& today, iListService& service,
           mediaType type, syncReport& report)
      : db_(db), today_(today), service_(service), type_(type), report_(report) {}

  // Returns whether anything changed locally.
  bool run() {
    if (!fetch())
      return false;
    linkRemoteRows();
    std::map<int, entry> locals;
    for (const auto& item : db_.getEntries(type_)) {
      if (auto id = service_.idOf(item))
        locals.emplace(*id, item);
    }
    std::set<int> ids;
    for (const auto* byId : {&locals, &bases_})
      for (const auto& [id, item] : *byId)
        ids.insert(id);
    for (const auto& [id, row] : remote_)
      ids.insert(id);
    for (int id : ids)
      settle(id, findId(locals, id), findId(bases_, id), findId(remote_, id));
    return changedLocally_;
  }

private:
  bool fetch() {
    auto fetched = service_.fetchList(type_, std::nullopt);
    if (!fetched.ok()) {
      problem(fetched.errorMessage());
      return false;
    }
    for (auto& row : fetched.value()) {
      if (auto id = service_.idOf(row.item))
        remote_[*id] = std::move(row);
    }
    bases_ = db_.syncBases(service_.key(), type_);
    if (remote_.empty() && !bases_.empty()) {
      problem("the service sent an empty list; nothing was changed.");
      return false;
    }
    return true;
  }

  // A remote row may match a local entry through the other service's id;
  // that entry then gains this service's id.
  void linkRemoteRows() {
    for (const auto& [id, row] : remote_) {
      auto local = findByAnyId(db_, row.item);
      if (!local || service_.idOf(*local) == id)
        continue;
      adoptServiceIds(*local, row.item);
      db_.updateEntry(*local);
    }
  }

  void settle(int id, const entry* local, const entry* base,
              const remoteListEntry* remote) {
    if (local && remote) {
      settleBoth(id, *local, base, remote->item);
    } else if (local) {
      settleLocalOnly(id, *local, base);
    } else if (remote && base) {
      deleteThere(id, *remote);
    } else if (remote) {
      addHere(id, remote->item);
    } else {
      db_.eraseSyncBase(service_.key(), type_, id);
    }
  }

  void settleBoth(int id, const entry& local, const entry* base,
                  const entry& remote) {
    entry merged = local;
    if (base) {
      std::vector<std::string> clashes;
      merged = mergeWithBase(local, *base, remote, clashes);
      if (!clashes.empty())
        problem("'" + local.title + "' changed here and there (" +
                joined(clashes) + "); kept this copy.");
    } else if (!localIsNewer(local, remote)) {
      copyListFields(merged, remote);
    }
    takeServiceDetails(merged, remote);
    bool agreed = sameListFields(merged, remote) || push(merged);
    keepLocally(local, merged);
    if (agreed)
      db_.setSyncBase(service_.key(), id, merged);
  }

  void settleLocalOnly(int id, const entry& local, const entry* base) {
    if (base && sameListFields(local, *base)) {
      LOG_INFO("Removed '" + local.title + "': deleted on " +
               service_.displayName());
      db_.removeEntry(type_, local.id);
      db_.eraseSyncBase(service_.key(), type_, id);
      ++report_.removedLocally;
      changedLocally_ = true;
      return;
    }
    if (base)
      problem("'" + local.title + "' was deleted there after it changed "
              "here; sent it back.");
    if (push(local))
      db_.setSyncBase(service_.key(), id, local);
  }

  void deleteThere(int id, const remoteListEntry& remote) {
    auto outcome = service_.deleteEntry(remote);
    if (!outcome.ok()) {
      problem(outcome.errorMessage());
      return;
    }
    db_.eraseSyncBase(service_.key(), type_, id);
    ++report_.removedRemotely;
  }

  void addHere(int id, const entry& remote) {
    db_.addEntry(newLocalEntry(remote));
    db_.setSyncBase(service_.key(), id, remote);
    ++report_.added;
    changedLocally_ = true;
  }

  bool push(const entry& item) {
    auto outcome = service_.saveEntry(item);
    if (!outcome.ok()) {
      problem(outcome.errorMessage());
      return false;
    }
    ++report_.pushed;
    return true;
  }

  void keepLocally(const entry& before, entry after) {
    if (!sameDetails(before, after)) {
      after.updatedAt = today_;
      ++report_.updated;
      changedLocally_ = true;
    }
    db_.updateEntry(after);
  }

  void problem(const std::string& message) {
    report_.problems.push_back(serviceProblem(service_, type_, message));
  }

  database& db_;
  const isoDate& today_;
  iListService& service_;
  mediaType type_;
  syncReport& report_;
  std::map<int, remoteListEntry> remote_;
  std::map<int, entry> bases_;
  bool changedLocally_ = false;
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
  // A second round carries what a later service changed locally over to the
  // earlier one in the same run.
  for (int round = 0; round < 2; ++round) {
    for (auto type : {mediaType::kAnime, mediaType::kManga})
      fillMissingIds(type, report, connected);
    bool changedLocally = false;
    for (auto* service : connected) {
      for (auto type : {mediaType::kAnime, mediaType::kManga})
        changedLocally |= syncPass(db_, today, *service, type, report).run();
    }
    if (!changedLocally || connected.size() < 2)
      break;
  }
  db_.saveAll();
  return report;
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
    if (sameDetails(merged, *existing))
      continue;
    db_.updateEntry(merged);
    ++report.updated;
  }
  db_.saveAll();
  return report;
}

}  // namespace catalist
