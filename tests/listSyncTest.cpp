#include <doctest/doctest.h>

#include <map>

#include "catalist/net/listSync.hpp"
#include "catalist/storage/backup.hpp"
#include "support/testSupport.hpp"

using catalist::entry;
using catalist::mediaType;
using catalist::watchStatus;
using catalist::test::makeEntry;

namespace {

// A list service held in memory, keyed by its own id. `idKey` decides which
// id it uses: "mal" or "anilist".
class fakeListService : public catalist::iListService {
public:
  explicit fakeListService(std::string idKey) : key_(std::move(idKey)) {}

  std::string key() const override { return key_; }
  std::string displayName() const override { return key_; }
  bool isConnected() const override { return connected; }

  catalist::result<std::vector<catalist::remoteListEntry>> fetchList(
      mediaType type, const std::optional<std::string>& userName) override {
    lastUser = userName;
    if (failFetch)
      return catalist::result<std::vector<catalist::remoteListEntry>>::failure("offline");
    std::vector<catalist::remoteListEntry> rows;
    for (const auto& [id, item] : rows_) {
      if (item.type == type)
        rows.push_back({item, id});
    }
    return catalist::result<std::vector<catalist::remoteListEntry>>::success(rows);
  }

  catalist::syncOutcome saveEntry(const entry& item) override {
    if (failSave)
      return catalist::syncOutcome::failure("refused");
    entry stored = item;
    stored.dirty = false;
    stored.syncedWith.clear();
    rows_[*idOf(item)] = stored;
    saved.push_back(item.title);
    return catalist::syncSucceeded();
  }

  catalist::syncOutcome deleteEntry(const catalist::remoteListEntry& row) override {
    rows_.erase(*idOf(row.item));
    deleted.push_back(row.item.title);
    return catalist::syncSucceeded();
  }

  std::optional<int> idOf(const entry& item) const override {
    int id = key_ == "mal" ? item.malId : item.anilistId;
    if (id > 0)
      return id;
    return std::nullopt;
  }

  void put(entry item) {
    item.dirty = false;
    item.syncedWith.clear();
    rows_[*idOf(item)] = item;
  }
  bool has(int id) const { return rows_.count(id) > 0; }
  const entry& row(int id) const { return rows_.at(id); }

  bool connected = true;
  bool failFetch = false;
  bool failSave = false;
  std::optional<std::string> lastUser;
  std::vector<std::string> saved;
  std::vector<std::string> deleted;

private:
  std::string key_;
  std::map<int, entry> rows_;
};

class fakeMapper : public catalist::iIdMapper {
public:
  catalist::result<std::map<int, int>> anilistIdsForMal(
      mediaType, const std::vector<int>& malIds) override {
    std::map<int, int> mapped;
    for (int id : malIds)
      mapped[id] = id + 100000;
    return catalist::result<std::map<int, int>>::success(mapped);
  }
  catalist::result<std::map<int, int>> malIdsForAnilist(
      mediaType, const std::vector<int>& anilistIds) override {
    std::map<int, int> mapped;
    for (int id : anilistIds) {
      if (id > 100000)
        mapped[id] = id - 100000;
    }
    return catalist::result<std::map<int, int>>::success(mapped);
  }
};

struct syncFixture {
  catalist::memoryFileSystem fs;
  catalist::fixedClock clock = catalist::test::makeTestClock();
  catalist::database db{fs, clock, "/data"};
  fakeListService mal{"mal"};
  fakeListService anilist{"anilist"};
  fakeMapper mapper;

  catalist::syncReport syncMalOnly() {
    anilist.connected = false;
    return catalist::listSync(db, clock, {&mal, &anilist}, &mapper).run();
  }
  catalist::syncReport syncBoth() {
    return catalist::listSync(db, clock, {&mal, &anilist}, &mapper).run();
  }
  entry local(int id) { return *db.getById(mediaType::kAnime, id); }
};

}  // namespace

TEST_CASE_FIXTURE(syncFixture, "sync needs at least one connected service") {
  mal.connected = false;
  anilist.connected = false;
  auto report = syncBoth();
  CHECK_FALSE(report.ok());
  CHECK(report.problems.front() == "Log in to MyAnimeList or AniList first.");
}

TEST_CASE_FIXTURE(syncFixture, "sync adds remote rows and remembers the service") {
  REQUIRE(db.saveAll());
  mal.put(makeEntry().malId(5).title("Remote").status(watchStatus::kCompleted).score(8));
  auto report = syncMalOnly();
  CHECK(report.ok());
  CHECK(report.added == 1);
  auto added = local(5);
  CHECK(added.title == "Remote");
  CHECK(added.score == 8);
  CHECK(added.syncedWith == std::set<std::string>{"mal"});
  CHECK_FALSE(added.dirty);
  CHECK(fs.exists("/data/anime.csv"));
  CHECK_FALSE(catalist::backup::listSnapshots(fs, "/data").empty());
}

TEST_CASE_FIXTURE(syncFixture, "sync pushes local edits and then clears the dirty flag") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  syncMalOnly();
  entry edited = local(5);
  edited.progress = 4;
  edited.dirty = true;
  db.updateEntry(edited);

  auto report = syncMalOnly();
  CHECK(report.pushed == 1);
  CHECK(mal.row(5).progress == 4);
  CHECK_FALSE(local(5).dirty);
}

TEST_CASE_FIXTURE(syncFixture, "sync keeps an edit dirty when the push fails") {
  mal.put(makeEntry().malId(5).title("Show"));
  syncMalOnly();
  entry edited = local(5);
  edited.score = 9;
  edited.dirty = true;
  db.updateEntry(edited);
  mal.failSave = true;

  auto report = syncMalOnly();
  CHECK_FALSE(report.ok());
  CHECK(local(5).dirty);
}

TEST_CASE_FIXTURE(syncFixture, "sync pulls remote changes into clean synced entries") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  syncMalOnly();
  mal.put(makeEntry().malId(5).title("Show").progress(7).notes("from web"));
  auto report = syncMalOnly();
  CHECK(report.updated == 1);
  CHECK(local(5).progress == 7);
  CHECK(local(5).notes == "from web");
  CHECK(local(5).updatedAt->toString() == "2026-10-20");
}

TEST_CASE_FIXTURE(syncFixture, "sync on first contact lets the newer side win") {
  db.addEntry(makeEntry().id(5).malId(5).title("Newer here").progress(9).updatedAt("2026-10-01"));
  db.addEntry(makeEntry().id(6).malId(6).title("Older here").progress(1).updatedAt("2024-01-01"));
  mal.put(makeEntry().malId(5).title("Newer here").progress(2).updatedAt("2025-01-01"));
  mal.put(makeEntry().malId(6).title("Older here").progress(6).updatedAt("2025-01-01"));
  auto report = syncMalOnly();
  CHECK(mal.row(5).progress == 9);
  CHECK(local(6).progress == 6);
  CHECK(report.pushed == 1);
  CHECK(report.updated == 1);
}

TEST_CASE_FIXTURE(syncFixture, "sync sends entries the service has never seen") {
  db.addEntry(makeEntry().id(5).malId(5).title("Only here"));
  db.addEntry(makeEntry().id(10001).title("No ids"));
  auto report = syncMalOnly();
  CHECK(mal.has(5));
  CHECK(report.pushed == 1);
  CHECK(local(5).syncedWith.count("mal") == 1);
}

TEST_CASE_FIXTURE(syncFixture, "sync removes entries deleted remotely and deletes local removals remotely") {
  mal.put(makeEntry().malId(5).title("Deleted there"));
  mal.put(makeEntry().malId(6).title("Deleted here"));
  syncMalOnly();
  REQUIRE(mal.deleteEntry({makeEntry().malId(5).title("Deleted there"), std::nullopt}).ok());
  REQUIRE(db.removeEntry(mediaType::kAnime, 6));

  auto report = syncMalOnly();
  CHECK_FALSE(db.getById(mediaType::kAnime, 5).has_value());
  CHECK_FALSE(db.getById(mediaType::kAnime, 6).has_value());
  CHECK_FALSE(mal.has(6));
  CHECK(report.removedLocally == 1);
  CHECK(report.removedRemotely == 1);
  CHECK(db.pendingDeletions().size() == 0);
}

TEST_CASE_FIXTURE(syncFixture, "sync failing to read a list changes nothing for it") {
  db.addEntry(makeEntry().id(5).malId(5).title("Synced").dirty(true));
  mal.failFetch = true;
  auto report = syncMalOnly();
  CHECK_FALSE(report.ok());
  CHECK(report.problems.front() == "mal (Anime): offline");
  CHECK(local(5).dirty);
  CHECK(mal.saved.empty());
}

TEST_CASE_FIXTURE(syncFixture, "sync carries entries from one service to the other through the id map") {
  mal.put(makeEntry().malId(5).title("From MAL"));
  anilist.put(makeEntry().anilistId(100009).malId(9).title("From AniList"));
  auto report = syncBoth();
  CHECK(report.ok());
  CHECK(anilist.has(100005));
  CHECK(mal.has(9));
  auto fromMal = local(5);
  CHECK(fromMal.anilistId == 100005);
  CHECK(fromMal.syncedWith == std::set<std::string>{"anilist", "mal"});
}

TEST_CASE_FIXTURE(syncFixture, "import adds new rows and fills only what the user left empty") {
  db.addEntry(makeEntry().id(5).malId(5).title("Mine").score(9).progress(3));
  mal.put(makeEntry().malId(5).title("Mine").score(4).progress(8).notes("theirs").totalUnits(12));
  mal.put(makeEntry().malId(6).title("New"));
  mal.connected = false;
  auto report = catalist::listSync(db, clock, {&mal}, nullptr)
                    .importFrom(mal, mediaType::kAnime, std::string("friend"));
  CHECK(report.ok());
  CHECK(mal.lastUser == "friend");
  CHECK(report.added == 1);
  CHECK(report.updated == 1);
  auto mine = local(5);
  CHECK(mine.score == 9);
  CHECK(mine.progress == 3);
  CHECK(mine.notes == "theirs");
  CHECK(mine.totalUnits == 12);
  CHECK(mine.syncedWith.empty());
  CHECK(mal.saved.empty());
}
