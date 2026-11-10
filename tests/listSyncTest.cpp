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
    if (answerEmpty)
      return catalist::result<std::vector<catalist::remoteListEntry>>::success(rows);
    for (const auto& [id, item] : rows_) {
      if (item.type == type)
        rows.push_back({item, id});
    }
    return catalist::result<std::vector<catalist::remoteListEntry>>::success(rows);
  }

  catalist::syncOutcome saveEntry(const entry& item) override {
    if (failSave)
      return catalist::syncOutcome::failure("refused");
    rows_[*idOf(item)] = item;
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

  void put(const entry& item) { rows_[*idOf(item)] = item; }
  bool has(int id) const { return rows_.count(id) > 0; }
  const entry& row(int id) const { return rows_.at(id); }

  bool connected = true;
  bool failFetch = false;
  bool failSave = false;
  bool answerEmpty = false;
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
  std::map<int, entry> bases(const std::string& service) {
    return db.syncBases(service, mediaType::kAnime);
  }
  void edit(int id, int progress, int score) {
    entry edited = local(id);
    edited.progress = progress;
    edited.score = score;
    db.updateEntry(edited);
  }
};

}  // namespace

TEST_CASE_FIXTURE(syncFixture, "sync needs at least one connected service") {
  mal.connected = false;
  anilist.connected = false;
  auto report = syncBoth();
  CHECK_FALSE(report.ok());
  CHECK(report.problems.front() == "Log in to MyAnimeList or AniList first.");
}

// Local, base and remote: the rows of the table in ADR-012.

TEST_CASE_FIXTURE(syncFixture, "sync adds remote rows and remembers the agreed values") {
  REQUIRE(db.saveAll());
  mal.put(makeEntry().malId(5).title("Remote").status(watchStatus::kCompleted).score(8));
  auto report = syncMalOnly();
  CHECK(report.ok());
  CHECK(report.added == 1);
  auto added = local(5);
  CHECK(added.title == "Remote");
  CHECK(added.score == 8);
  REQUIRE(bases("mal").count(5) == 1);
  CHECK(bases("mal").at(5).score == 8);
  CHECK(fs.exists("/data/anime.csv"));
  CHECK(fs.exists("/data/sync.csv"));
  CHECK_FALSE(catalist::backup::listSnapshots(fs, "/data").empty());
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
  CHECK(bases("mal").at(5).progress == 9);
  CHECK(bases("mal").at(6).progress == 6);
}

TEST_CASE_FIXTURE(syncFixture, "sync on first contact with equal values sends nothing") {
  db.addEntry(makeEntry().id(5).malId(5).title("Same").progress(3));
  mal.put(makeEntry().malId(5).title("Same").progress(3));
  auto report = syncMalOnly();
  CHECK(report.pushed == 0);
  CHECK(report.updated == 0);
  CHECK(bases("mal").count(5) == 1);
}

TEST_CASE_FIXTURE(syncFixture, "sync sends entries the service has never seen") {
  db.addEntry(makeEntry().id(5).malId(5).title("Only here"));
  db.addEntry(makeEntry().id(10001).title("No ids"));
  auto report = syncMalOnly();
  CHECK(mal.has(5));
  CHECK(report.pushed == 1);
  CHECK(bases("mal").count(5) == 1);
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
  CHECK(bases("mal").empty());
}

TEST_CASE_FIXTURE(syncFixture, "sync sends back an entry deleted there after it changed here") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  mal.put(makeEntry().malId(6).title("Other"));
  syncMalOnly();
  REQUIRE(mal.deleteEntry({makeEntry().malId(5), std::nullopt}).ok());
  edit(5, 2, 0);

  auto report = syncMalOnly();
  REQUIRE(mal.has(5));
  CHECK(mal.row(5).progress == 2);
  CHECK(local(5).progress == 2);
  REQUIRE(report.problems.size() == 1);
  CHECK(report.problems.front() ==
        "mal (Anime): 'Show' was deleted there after it changed here; sent it back.");
}

TEST_CASE_FIXTURE(syncFixture, "sync forgets a base once the title is gone from both sides") {
  mal.put(makeEntry().malId(5).title("Show"));
  mal.put(makeEntry().malId(6).title("Other"));
  syncMalOnly();
  REQUIRE(db.removeEntry(mediaType::kAnime, 5));
  REQUIRE(mal.deleteEntry({makeEntry().malId(5), std::nullopt}).ok());

  auto report = syncMalOnly();
  CHECK(report.ok());
  CHECK(report.removedRemotely == 0);
  CHECK(bases("mal").count(5) == 0);
  CHECK(bases("mal").count(6) == 1);
}

TEST_CASE_FIXTURE(syncFixture, "sync keeps an entry whose delete was undone before syncing") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  syncMalOnly();
  entry kept = local(5);
  REQUIRE(db.removeEntry(mediaType::kAnime, 5));
  db.addEntry(kept);

  auto report = syncMalOnly();
  CHECK(report.ok());
  CHECK(mal.has(5));
  CHECK(db.getById(mediaType::kAnime, 5).has_value());
  CHECK(mal.deleted.empty());
}

// Merging one field of a title both sides know.

TEST_CASE_FIXTURE(syncFixture, "sync pulls remote changes into entries unchanged here") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  syncMalOnly();
  mal.put(makeEntry().malId(5).title("Show").progress(7).notes("from web"));
  auto report = syncMalOnly();
  CHECK(report.updated == 1);
  CHECK(local(5).progress == 7);
  CHECK(local(5).notes == "from web");
  CHECK(local(5).updatedAt->toString() == "2026-10-20");
}

TEST_CASE_FIXTURE(syncFixture, "sync pushes local edits and remembers them as agreed") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  syncMalOnly();
  edit(5, 4, 0);

  auto report = syncMalOnly();
  CHECK(report.pushed == 1);
  CHECK(mal.row(5).progress == 4);
  CHECK(bases("mal").at(5).progress == 4);
  CHECK(syncMalOnly().pushed == 0);
}

TEST_CASE_FIXTURE(syncFixture, "sync keeps the local value when both sides changed a field") {
  mal.put(makeEntry().malId(5).title("Show").progress(1).score(5));
  syncMalOnly();
  mal.put(makeEntry().malId(5).title("Show").progress(1).score(6));
  edit(5, 1, 9);

  auto report = syncMalOnly();
  CHECK(local(5).score == 9);
  CHECK(mal.row(5).score == 9);
  REQUIRE(report.problems.size() == 1);
  CHECK(report.problems.front() ==
        "mal (Anime): 'Show' changed here and there (score); kept this copy.");
}

TEST_CASE_FIXTURE(syncFixture, "sync keeps a remote edit to a field the local edit left alone") {
  mal.put(makeEntry().malId(5).title("Show").progress(1).score(0));
  syncMalOnly();
  mal.put(makeEntry().malId(5).title("Show").progress(1).score(9));
  edit(5, 2, 0);

  syncMalOnly();
  CHECK(mal.row(5).score == 9);
  CHECK(mal.row(5).progress == 2);
  CHECK(local(5).score == 9);
}

// Failures.

TEST_CASE_FIXTURE(syncFixture, "sync failing to read a list changes nothing for it") {
  db.addEntry(makeEntry().id(5).malId(5).title("Synced"));
  mal.failFetch = true;
  auto report = syncMalOnly();
  CHECK_FALSE(report.ok());
  CHECK(report.problems.front() == "mal (Anime): offline");
  CHECK(mal.saved.empty());
  CHECK(bases("mal").empty());
}

TEST_CASE_FIXTURE(syncFixture, "sync does not trust an empty list while entries are synced") {
  for (int id = 1; id <= 5; ++id)
    mal.put(makeEntry().malId(id).title("Show " + std::to_string(id)));
  syncMalOnly();
  mal.answerEmpty = true;
  auto report = syncMalOnly();
  CHECK(db.getEntries(mediaType::kAnime).size() == 5);
  CHECK_FALSE(report.ok());
}

TEST_CASE_FIXTURE(syncFixture, "sync keeps an edit for the next run when the push fails") {
  mal.put(makeEntry().malId(5).title("Show"));
  syncMalOnly();
  edit(5, 0, 9);
  mal.failSave = true;

  auto report = syncMalOnly();
  CHECK_FALSE(report.ok());
  CHECK(bases("mal").at(5).score == 0);

  mal.failSave = false;
  CHECK(syncMalOnly().pushed == 1);
  CHECK(mal.row(5).score == 9);
}

// Two services.

TEST_CASE_FIXTURE(syncFixture, "sync carries entries from one service to the other through the id map") {
  mal.put(makeEntry().malId(5).title("From MAL"));
  anilist.put(makeEntry().anilistId(100009).malId(9).title("From AniList"));
  auto report = syncBoth();
  CHECK(report.ok());
  CHECK(anilist.has(100005));
  CHECK(mal.has(9));
  auto fromMal = local(5);
  CHECK(fromMal.anilistId == 100005);
  CHECK(bases("mal").count(5) == 1);
  CHECK(bases("anilist").count(100005) == 1);
}

TEST_CASE_FIXTURE(syncFixture, "sync carries a remote edit to the local copy and the other service") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  anilist.put(makeEntry().anilistId(100005).malId(5).title("Show").progress(1));
  syncBoth();
  mal.put(makeEntry().malId(5).title("Show").progress(7));
  syncBoth();
  CHECK(local(5).progress == 7);
  CHECK(anilist.row(100005).progress == 7);
}

TEST_CASE_FIXTURE(syncFixture, "sync carries an edit from the later service back to the earlier one") {
  mal.put(makeEntry().malId(5).title("Show").progress(1));
  anilist.put(makeEntry().anilistId(100005).malId(5).title("Show").progress(1));
  syncBoth();
  anilist.put(makeEntry().anilistId(100005).malId(5).title("Show").progress(4));
  auto report = syncBoth();
  CHECK(report.ok());
  CHECK(local(5).progress == 4);
  CHECK(mal.row(5).progress == 4);
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
  CHECK(bases("mal").empty());
  CHECK(mal.saved.empty());
}
