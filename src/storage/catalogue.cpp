#include "storage/catalogue.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <utility>

#include "core/entry.hpp"

namespace catalist {

namespace {

const char* kStoreFile = "catalist.db";
const char* kProviderKey = "provider";

// PRAGMA user_version of a store this code set up; 0 is a new, empty file.
constexpr int kStoreVersion = 1;

// `entry` holds both lists. `tx` and `change` are the undo history: one
// row per command that changed something, and under it each entry before
// and after, as csvCodec rows (empty: the entry did not exist).
const char* kSchema =
    "CREATE TABLE entry ("
    " type TEXT NOT NULL, id INTEGER NOT NULL, mal_id INTEGER NOT NULL,"
    " title TEXT NOT NULL, status TEXT NOT NULL, score INTEGER NOT NULL,"
    " progress INTEGER NOT NULL, total_units INTEGER NOT NULL,"
    " volumes INTEGER NOT NULL, total_volumes INTEGER NOT NULL,"
    " rewatched INTEGER NOT NULL, start_date TEXT, finish_date TEXT,"
    " updated_at TEXT, released INTEGER NOT NULL, checked_at TEXT,"
    " anilist_id INTEGER NOT NULL, PRIMARY KEY (type, id));"
    "CREATE TABLE setting (key TEXT PRIMARY KEY, value TEXT NOT NULL);"
    "CREATE TABLE tx ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT, stamp TEXT NOT NULL,"
    " label TEXT NOT NULL, undone INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE change ("
    " tx INTEGER NOT NULL, seq INTEGER NOT NULL, type TEXT NOT NULL,"
    " id INTEGER NOT NULL, before TEXT NOT NULL, after TEXT NOT NULL,"
    " PRIMARY KEY (tx, seq));"
    "PRAGMA user_version = 1;";

const char* kSelectEntries =
    "SELECT type, id, mal_id, title, status, score, progress, total_units,"
    " volumes, total_volumes, rewatched, start_date, finish_date, updated_at,"
    " released, checked_at, anilist_id FROM entry ORDER BY type, id";
const char* kInsertEntry =
    "INSERT OR REPLACE INTO entry VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
const char* kInsertSetting = "INSERT OR REPLACE INTO setting VALUES (?, ?)";

// The ids a title has on sites `entry` has no column for (Kitsu's), kept
// beside it: Catalist-TUI shares the file and writes `entry` with exactly
// its columns. A row also holds the MyAnimeList and AniList ids the title
// had when it was written, so that a row whose title the TUI removed, and
// whose ref it gave to another, is not read as that other title's.
const char* kSiteIdTable =
    "CREATE TABLE IF NOT EXISTS entry_id ("
    " type TEXT NOT NULL, id INTEGER NOT NULL, site TEXT NOT NULL,"
    " site_id INTEGER NOT NULL, mal_id INTEGER NOT NULL, anilist_id INTEGER NOT NULL,"
    " PRIMARY KEY (type, id, site))";

// How long peek waits for another program's write: not long enough to be
// felt at the prompt.
constexpr int kPeekWaitMs = 100;

int storeVersion(const store::database& db) {
	auto version = db.prepare("PRAGMA user_version");
	return version.step() ? static_cast<int>(version.intAt(0)) : 0;
}

void bindDate(store::statement& into, const std::optional<isoDate>& date) {
	if (date)
		into.bind(date->toString());
	else
		into.bindNull();
}

// In the order of kSelectEntries.
store::statement& bindEntry(store::statement& into, const entry& row) {
	into.bind(typeKey(row.type)).bind(row.id).bind(row.malId).bind(row.title);
	into.bind(statusKey(row.status)).bind(row.score).bind(row.progress).bind(row.totalUnits);
	into.bind(row.volumes).bind(row.totalVolumes).bind(row.rewatched);
	bindDate(into, row.startDate);
	bindDate(into, row.finishDate);
	bindDate(into, row.updatedAt);
	into.bind(row.released);
	bindDate(into, row.checkedAt);
	return into.bind(row.anilistId);
}

entry readEntry(const store::statement& from) {
	auto number = [&from](int column) { return static_cast<int>(from.intAt(column)); };
	entry row;
	row.type = from.textAt(0) == "manga" ? mediaType::kManga : mediaType::kAnime;
	row.id = number(1);
	row.malId = number(2);
	row.title = from.textAt(3);
	row.status = parseStatus(from.textAt(4)).value_or(watchStatus::kPlanToWatch);
	row.score = number(5);
	row.progress = number(6);
	row.totalUnits = number(7);
	row.volumes = number(8);
	row.totalVolumes = number(9);
	row.rewatched = number(10);
	row.startDate = isoDate::parse(from.textAt(11));
	row.finishDate = isoDate::parse(from.textAt(12));
	row.updatedAt = isoDate::parse(from.textAt(13));
	row.released = number(14);
	row.checkedAt = isoDate::parse(from.textAt(15));
	row.anilistId = number(16);
	return row;
}

bool hasTable(const store::database& db, const std::string& name) {
	auto found = db.prepare("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?");
	return found.bind(name).step();
}

// Gives each entry its Kitsu id from entry_id, where the row is still the
// entry's own.
bool readSiteIds(const store::database& db, std::vector<entry>& anime, std::vector<entry>& manga) {
	// A file no Catalist with Kitsu has written to yet.
	if (not hasTable(db, "entry_id"))
		return true;
	auto rows = db.prepare(
	    "SELECT type, id, site_id, mal_id, anilist_id FROM entry_id WHERE site = 'kitsu'");
	while (rows.step()) {
		auto& list = rows.textAt(0) == "manga" ? manga : anime;
		int id = static_cast<int>(rows.intAt(1));
		auto found =
		    std::find_if(list.begin(), list.end(), [id](const entry& e) { return e.id == id; });
		bool same = found != list.end() and found->malId == rows.intAt(3) and
		            found->anilistId == rows.intAt(4);
		if (same)
			found->kitsuId = static_cast<int>(rows.intAt(2));
	}
	return rows.ok();
}

// Titles the sites are still to be told of: kept beside the lists, so a
// change made offline is sent the next time a site can be reached.
// `fields` (what to send), `attempts` and `next_try` came later: an outbox
// made before them gets them added (addOutboxColumns).
const char* kOutboxTable =
    "CREATE TABLE IF NOT EXISTS outbox ("
    " seq INTEGER PRIMARY KEY AUTOINCREMENT, site TEXT NOT NULL, type TEXT NOT NULL,"
    " id INTEGER NOT NULL, removed INTEGER NOT NULL, site_id INTEGER NOT NULL,"
    " fields INTEGER NOT NULL DEFAULT 127, attempts INTEGER NOT NULL DEFAULT 0,"
    " next_try INTEGER NOT NULL DEFAULT 0, UNIQUE (site, type, id, removed))";
static_assert(siteField::kAll == 127, "the outbox's default fields are every field");

bool hasColumn(const store::database& db, const std::string& table, const std::string& column) {
	auto found = db.prepare("SELECT 1 FROM pragma_table_info(?) WHERE name = ?");
	return found.bind(table).bind(column).step();
}

// The outbox as this program reads it, made or brought up to date.
bool addOutboxColumns(store::database& db) {
	if (not db.run(kOutboxTable))
		return false;
	for (const char* column : {"fields INTEGER NOT NULL DEFAULT 127",
	                           "attempts INTEGER NOT NULL DEFAULT 0",
	                           "next_try INTEGER NOT NULL DEFAULT 0"}) {
		std::string name(column, std::string_view(column).find(' '));
		if (not hasColumn(db, "outbox", name) and
		    not db.run(std::format("ALTER TABLE outbox ADD COLUMN {}", column)))
			return false;
	}
	return true;
}

const entry* findById(const std::vector<entry>& list, int id) {
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const entry& e) { return e.id == id; });
	return found == list.end() ? nullptr : &*found;
}

// A local id only numbers a title, so two titles are the same only when a
// site's id says so.
bool sameTitle(const entry& existing, const entry& incoming) {
	return (incoming.anilistId > 0 and existing.anilistId == incoming.anilistId) or
	       (incoming.malId > 0 and existing.malId == incoming.malId) or
	       (incoming.kitsuId > 0 and existing.kitsuId == incoming.kitsuId);
}

int nextId(const std::vector<entry>& list) {
	auto highest = std::max_element(list.begin(), list.end(),
	                                [](const entry& a, const entry& b) { return a.id < b.id; });
	return highest == list.end() ? 1 : highest->id + 1;
}

}  // namespace

std::filesystem::path dataDirFor(const dataDirEnv& env) {
	if (not env.catalistDataDir.empty())
		return env.catalistDataDir;
	// The XDG specification: a relative path there is invalid and ignored.
	std::filesystem::path xdg = env.xdgDataHome;
	if (xdg.is_absolute())
		return xdg / "catalist";
	if (not env.home.empty())
		return std::filesystem::path(env.home) / ".local" / "share" / "catalist";
	return "catalist-data";
}

catalogue::catalogue(std::filesystem::path dataDir) : dataDir_(std::move(dataDir)) {}

std::filesystem::path catalogue::getDataDir() const {
	return dataDir_;
}

std::filesystem::path catalogue::getStorePath() const {
	return dataDir_ / kStoreFile;
}

std::vector<entry>& catalogue::getList(mediaType type) {
	return type == mediaType::kAnime ? animeEntries_ : mangaEntries_;
}

const std::vector<entry>& catalogue::getList(mediaType type) const {
	return type == mediaType::kAnime ? animeEntries_ : mangaEntries_;
}

const std::vector<entry>& catalogue::getSaved(mediaType type) const {
	return type == mediaType::kAnime ? savedAnime_ : savedManga_;
}

int catalogue::addEntry(entry added) {
	auto& list = getList(added.type);
	for (auto& existing : list) {
		if (not sameTitle(existing, added))
			continue;
		existing = mergeImported(existing, added);
		return existing.id;
	}
	added.id = nextId(list);
	list.push_back(std::move(added));
	return list.back().id;
}

const std::vector<entry>& catalogue::getEntries(mediaType type) const {
	return getList(type);
}

std::optional<entry> catalogue::getById(mediaType type, int id) const {
	const auto& list = getList(type);
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const entry& e) { return e.id == id; });
	return found == list.end() ? std::nullopt : std::optional<entry>(*found);
}

std::optional<entry> catalogue::findByIds(mediaType type, int anilistId, int malId,
                                          int kitsuId) const {
	entry wanted;
	wanted.anilistId = anilistId;
	wanted.malId = malId;
	wanted.kitsuId = kitsuId;
	const auto& list = getList(type);
	auto found = std::find_if(list.begin(), list.end(),
	                          [&wanted](const entry& e) { return sameTitle(e, wanted); });
	return found == list.end() ? std::nullopt : std::optional<entry>(*found);
}

bool catalogue::updateEntry(const entry& changed) {
	auto& list = getList(changed.type);
	auto found = std::find_if(list.begin(), list.end(),
	                          [&changed](const entry& e) { return e.id == changed.id; });
	if (found == list.end())
		return false;
	*found = changed;
	return true;
}

bool catalogue::removeEntry(mediaType type, int id) {
	auto& list = getList(type);
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const entry& e) { return e.id == id; });
	if (found == list.end())
		return false;
	list.erase(found);
	return true;
}

bool catalogue::restoreEntry(const entry& back) {
	auto& list = getList(back.type);
	if (findById(list, back.id))
		return false;
	// In its place by id, the order the lists are read in.
	auto after =
	    std::find_if(list.begin(), list.end(), [&back](const entry& e) { return e.id > back.id; });
	list.insert(after, back);
	return true;
}

provider catalogue::getProvider() const {
	return provider_;
}

void catalogue::setProvider(provider site) {
	provider_ = site;
}

// What the sites are still to be told.

std::vector<pending> catalogue::outbox() {
	std::vector<pending> waiting;
	// No lists yet: nothing waits, and nothing is made to say so.
	std::error_code ignored;
	if (not db_.isOpen() and not std::filesystem::exists(getStorePath(), ignored))
		return waiting;
	if (not openStore().has_value() or not hasTable(db_, "outbox"))
		return waiting;
	// An outbox no write of this version has brought up to date yet.
	bool current = hasColumn(db_, "outbox", "next_try");
	auto rows = db_.prepare(
	    current ? "SELECT site, type, id, removed, site_id, fields, attempts, next_try FROM outbox"
	              " ORDER BY seq"
	            : "SELECT site, type, id, removed, site_id, 127, 0, 0 FROM outbox ORDER BY seq");
	while (rows.step()) {
		auto site = parseProvider(rows.textAt(0));
		if (not site)
			continue;
		waiting.push_back({*site, rows.textAt(1) == "manga" ? mediaType::kManga : mediaType::kAnime,
		                   static_cast<int>(rows.intAt(2)), rows.intAt(3) != 0,
		                   static_cast<int>(rows.intAt(4)), static_cast<unsigned>(rows.intAt(5)),
		                   static_cast<int>(rows.intAt(6)), rows.intAt(7)});
	}
	return waiting;
}

bool catalogue::queue(const std::vector<pending>& more) {
	return commit([this, &more] {
		if (not addOutboxColumns(db_))
			return false;
		auto known = db_.prepare(
		    "SELECT fields FROM outbox WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		auto update = db_.prepare(
		    "UPDATE outbox SET fields = ?, site_id = ?, attempts = 0, next_try = 0"
		    " WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		auto insert = db_.prepare(
		    "INSERT INTO outbox (site, type, id, removed, site_id, fields) VALUES (?, ?, ?, ?, ?, ?)");
		for (const auto& each : more) {
			std::string site = providerKey(each.site);
			std::string type = typeKey(each.type);
			int removed = each.removed ? 1 : 0;
			known.bind(site).bind(type).bind(each.id).bind(removed);
			bool waiting = known.step();
			unsigned fields = waiting ? mergeFields(static_cast<unsigned>(known.intAt(0)), each.fields)
			                          : each.fields;
			known.reset();
			bool done = waiting ? update.bind(static_cast<long long>(fields)).bind(each.siteId)
			                          .bind(site).bind(type).bind(each.id).bind(removed).run()
			                    : insert.bind(site).bind(type).bind(each.id).bind(removed)
			                          .bind(each.siteId).bind(static_cast<long long>(fields)).run();
			if (not done)
				return false;
		}
		return true;
	});
}

bool catalogue::deferred(const pending& waiting, int attempts, long long nextTry) {
	return commit([this, &waiting, attempts, nextTry] {
		if (not addOutboxColumns(db_))
			return false;
		auto later = db_.prepare(
		    "UPDATE outbox SET attempts = ?, next_try = ?"
		    " WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		later.bind(attempts).bind(nextTry).bind(providerKey(waiting.site)).bind(typeKey(waiting.type));
		return later.bind(waiting.id).bind(waiting.removed ? 1 : 0).run();
	});
}

bool catalogue::sent(const pending& done) {
	return commit([this, &done] {
		auto remove = db_.prepare(
		    "DELETE FROM outbox WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		remove.bind(providerKey(done.site)).bind(typeKey(done.type)).bind(done.id);
		return remove.bind(done.removed ? 1 : 0).run();
	});
}

// Opening and setting up the store.

done catalogue::openStore() {
	if (db_.isOpen())
		return done{};
	std::error_code ignored;
	std::filesystem::create_directories(dataDir_, ignored);
	if (not db_.open(getStorePath()))
		return std::unexpected(
		    std::format("cannot open {}: {}", getStorePath().string(), db_.lastError()));
	auto ready = setUpStore();
	if (not ready.has_value())
		db_.close();
	return ready;
}

// A new file gets its tables, in one transaction: two programs starting at
// the same moment cannot both set it up.
done catalogue::setUpStore() {
	if (storeVersion(db_) == kStoreVersion)
		return done{};

	store::transaction setup(db_);
	if (not setup.active())
		return std::unexpected(
		    std::format("cannot write to {}: {}", getStorePath().string(), db_.lastError()));
	int version = storeVersion(db_);
	if (version == kStoreVersion)
		return done{};
	if (version != 0)
		return std::unexpected(
		    std::format("{} was written by a newer Catalist", getStorePath().string()));

	if (not db_.run(kSchema))
		return std::unexpected(
		    std::format("cannot set up {}: {}", getStorePath().string(), db_.lastError()));
	if (not setup.commit())
		return std::unexpected(
		    std::format("cannot write to {}: {}", getStorePath().string(), db_.lastError()));
	return done{};
}

done catalogue::beginWrite() {
	auto opened = openStore();
	if (not opened.has_value())
		return opened;
	write_.emplace(db_);
	if (write_->active())
		return done{};
	write_.reset();
	return std::unexpected(std::format("{} is in use by another program ({})",
	                                   getStorePath().string(), db_.lastError()));
}

// Reading.

result<bool> catalogue::loadAll() {
	std::error_code ignored;
	bool stored = db_.isOpen() or std::filesystem::exists(getStorePath(), ignored);
	if (not stored)
		return false;
	auto opened = openStore();
	if (not opened)
		return std::unexpected(opened.error());
	if (not readStore())
		return std::unexpected(
		    std::format("cannot read {}: {}", getStorePath().string(), db_.lastError()));
	return true;
}

bool catalogue::peek() {
	std::error_code ignored;
	if (db_.isOpen() or not std::filesystem::exists(getStorePath(), ignored))
		return false;
	if (not db_.open(getStorePath(), store::openMode::kReadOnly, kPeekWaitMs))
		return false;
	bool read = storeVersion(db_) == kStoreVersion and readStore();
	db_.close();
	return read;
}

bool catalogue::readStore() {
	std::vector<entry> anime;
	std::vector<entry> manga;
	auto rows = db_.prepare(kSelectEntries);
	while (rows.step()) {
		entry row = readEntry(rows);
		(row.type == mediaType::kAnime ? anime : manga).push_back(std::move(row));
	}
	provider site = provider::kAniList;
	// Other keys (Catalist-TUI's `linked`) are another program's and are
	// left as they are.
	auto settings = db_.prepare("SELECT key, value FROM setting");
	while (settings.step()) {
		if (settings.textAt(0) == kProviderKey)
			site = parseProvider(settings.textAt(1)).value_or(site);
	}
	// An error ends a statement the way its last row does.
	if (not rows.ok() or not settings.ok() or not readSiteIds(db_, anime, manga))
		return false;

	animeEntries_ = std::move(anime);
	mangaEntries_ = std::move(manga);
	provider_ = site;
	rememberSaved();
	return true;
}

void catalogue::rememberSaved() {
	savedAnime_ = animeEntries_;
	savedManga_ = mangaEntries_;
	savedProvider_ = provider_;
}

// Writing.

bool catalogue::commit(const std::function<bool()>& work) {
	if (not openStore().has_value())
		return false;
	if (not write_)
		write_.emplace(db_);
	bool done = write_->active() and work() and write_->commit();
	// A write that did not get that far is rolled back here.
	write_.reset();
	return done;
}

bool catalogue::writeRows() {
	auto insert = db_.prepare(kInsertEntry);
	auto remove = db_.prepare("DELETE FROM entry WHERE type = ? AND id = ?");
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		const auto& now = getList(type);
		const auto& before = getSaved(type);
		for (const auto& row : now) {
			const entry* was = findById(before, row.id);
			bool same = was and *was == row;
			if (not same and not bindEntry(insert, row).run())
				return false;
		}
		for (const auto& row : before) {
			bool gone = findById(now, row.id) == nullptr;
			if (gone and not remove.bind(typeKey(type)).bind(row.id).run())
				return false;
		}
	}
	return writeSiteIds();
}

// The Kitsu ids, written anew: the rows of titles that are gone go too.
bool catalogue::writeSiteIds() {
	if (not db_.run(kSiteIdTable) or not db_.run("DELETE FROM entry_id WHERE site = 'kitsu'"))
		return false;
	auto insert = db_.prepare("INSERT INTO entry_id VALUES (?, ?, 'kitsu', ?, ?, ?)");
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		for (const auto& row : getList(type)) {
			if (row.kitsuId <= 0)
				continue;
			insert.bind(typeKey(type)).bind(row.id).bind(row.kitsuId);
			if (not insert.bind(row.malId).bind(row.anilistId).run())
				return false;
		}
	}
	return true;
}

bool catalogue::writeSettings() {
	auto set = db_.prepare(kInsertSetting);
	// MyAnimeList as Catalist 0.3 wrote it; Catalist-TUI reads "kitsu" as
	// AniList, its default.
	std::string site = provider_ == provider::kMyAnimeList ? "myanimelist" : providerKey(provider_);
	return set.bind(std::string(kProviderKey)).bind(site).run();
}

bool catalogue::saveSettings() {
	if (not commit([this] { return writeSettings(); }))
		return false;
	savedProvider_ = provider_;
	return true;
}

bool catalogue::writeChanged() {
	bool settingsChanged = provider_ != savedProvider_;
	return writeRows() and (not settingsChanged or writeSettings());
}

bool catalogue::saveAll() {
	bool done = commit([this] { return writeChanged(); });
	if (done)
		rememberSaved();
	return done;
}

}  // namespace catalist
