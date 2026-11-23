#include "storage/catalogue.hpp"

#include <algorithm>
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
// and after, as csv_codec rows (empty: the entry did not exist).
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

int StoreVersion(const store::Database& db) {
	auto version = db.Prepare("PRAGMA user_version");
	return version.Step() ? static_cast<int>(version.IntAt(0)) : 0;
}

void BindDate(store::Statement& into, const std::optional<IsoDate>& date) {
	if (date)
		into.Bind(date->ToString());
	else
		into.BindNull();
}

// In the order of kSelectEntries.
store::Statement& BindEntry(store::Statement& into, const Entry& row) {
	into.Bind(TypeKey(row.type)).Bind(row.id).Bind(row.mal_id).Bind(row.title);
	into.Bind(StatusKey(row.status)).Bind(row.score).Bind(row.progress).Bind(row.total_units);
	into.Bind(row.volumes).Bind(row.total_volumes).Bind(row.rewatched);
	BindDate(into, row.start_date);
	BindDate(into, row.finish_date);
	BindDate(into, row.updated_at);
	into.Bind(row.released);
	BindDate(into, row.checked_at);
	return into.Bind(row.anilist_id);
}

Entry ReadEntry(const store::Statement& from) {
	auto number = [&from](int column) { return static_cast<int>(from.IntAt(column)); };
	Entry row;
	row.type = from.TextAt(0) == "manga" ? MediaType::kManga : MediaType::kAnime;
	row.id = number(1);
	row.mal_id = number(2);
	row.title = from.TextAt(3);
	row.status = ParseStatus(from.TextAt(4)).value_or(WatchStatus::kPlanToWatch);
	row.score = number(5);
	row.progress = number(6);
	row.total_units = number(7);
	row.volumes = number(8);
	row.total_volumes = number(9);
	row.rewatched = number(10);
	row.start_date = IsoDate::Parse(from.TextAt(11));
	row.finish_date = IsoDate::Parse(from.TextAt(12));
	row.updated_at = IsoDate::Parse(from.TextAt(13));
	row.released = number(14);
	row.checked_at = IsoDate::Parse(from.TextAt(15));
	row.anilist_id = number(16);
	return row;
}

bool HasTable(const store::Database& db, const std::string& name) {
	auto found = db.Prepare("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?");
	return found.Bind(name).Step();
}

// Gives each entry its Kitsu id from entry_id, where the row is still the
// entry's own.
bool ReadSiteIds(const store::Database& db, std::vector<Entry>& anime, std::vector<Entry>& manga) {
	// A file no Catalist with Kitsu has written to yet.
	if (not HasTable(db, "entry_id"))
		return true;
	auto rows = db.Prepare(
	    "SELECT type, id, site_id, mal_id, anilist_id FROM entry_id WHERE site = 'kitsu'");
	while (rows.Step()) {
		auto& list = rows.TextAt(0) == "manga" ? manga : anime;
		int id = static_cast<int>(rows.IntAt(1));
		auto found =
		    std::find_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
		bool same = found != list.end() and found->mal_id == rows.IntAt(3) and
		            found->anilist_id == rows.IntAt(4);
		if (same)
			found->kitsu_id = static_cast<int>(rows.IntAt(2));
	}
	return rows.Ok();
}

// Titles the sites are still to be told of: kept beside the lists, so a
// change made offline is sent the next time a site can be reached.
// `fields` (what to send), `attempts` and `next_try` came later: an outbox
// made before them gets them added (AddOutboxColumns).
const char* kOutboxTable =
    "CREATE TABLE IF NOT EXISTS outbox ("
    " seq INTEGER PRIMARY KEY AUTOINCREMENT, site TEXT NOT NULL, type TEXT NOT NULL,"
    " id INTEGER NOT NULL, removed INTEGER NOT NULL, site_id INTEGER NOT NULL,"
    " fields INTEGER NOT NULL DEFAULT 127, attempts INTEGER NOT NULL DEFAULT 0,"
    " next_try INTEGER NOT NULL DEFAULT 0, UNIQUE (site, type, id, removed))";
static_assert(site_field::kAll == 127, "the outbox's default fields are every field");

bool HasColumn(const store::Database& db, const std::string& table, const std::string& column) {
	auto found = db.Prepare("SELECT 1 FROM pragma_table_info(?) WHERE name = ?");
	return found.Bind(table).Bind(column).Step();
}

// The outbox as this program reads it, made or brought up to date.
bool AddOutboxColumns(store::Database& db) {
	if (not db.Run(kOutboxTable))
		return false;
	for (const char* column :
	     {"fields INTEGER NOT NULL DEFAULT 127", "attempts INTEGER NOT NULL DEFAULT 0",
	      "next_try INTEGER NOT NULL DEFAULT 0"}) {
		std::string name(column, std::string_view(column).find(' '));
		if (not HasColumn(db, "outbox", name) and
		    not db.Run(std::format("ALTER TABLE outbox ADD COLUMN {}", column)))
			return false;
	}
	return true;
}

const Entry* FindById(const std::vector<Entry>& list, int id) {
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
	return found == list.end() ? nullptr : &*found;
}

// A local id only numbers a title, so two titles are the same only when a
// site's id says so.
bool SameTitle(const Entry& existing, const Entry& incoming) {
	return (incoming.anilist_id > 0 and existing.anilist_id == incoming.anilist_id) or
	       (incoming.mal_id > 0 and existing.mal_id == incoming.mal_id) or
	       (incoming.kitsu_id > 0 and existing.kitsu_id == incoming.kitsu_id);
}

int NextId(const std::vector<Entry>& list) {
	auto highest = std::max_element(list.begin(), list.end(),
	                                [](const Entry& a, const Entry& b) { return a.id < b.id; });
	return highest == list.end() ? 1 : highest->id + 1;
}

}  // namespace

std::filesystem::path DataDirFor(const DataDirEnv& env) {
	if (not env.catalist_data_dir.empty())
		return env.catalist_data_dir;
	// The XDG specification: a relative path there is invalid and ignored.
	std::filesystem::path xdg = env.xdg_data_home;
	if (xdg.is_absolute())
		return xdg / "catalist";
	if (not env.home.empty())
		return std::filesystem::path(env.home) / ".local" / "share" / "catalist";
	return "catalist-data";
}

Catalogue::Catalogue(std::filesystem::path data_dir) : dataDir_(std::move(data_dir)) {}

std::filesystem::path Catalogue::data_dir() const {
	return dataDir_;
}

std::filesystem::path Catalogue::StorePath() const {
	return dataDir_ / kStoreFile;
}

std::vector<Entry>& Catalogue::GetList(MediaType type) {
	return type == MediaType::kAnime ? animeEntries_ : mangaEntries_;
}

const std::vector<Entry>& Catalogue::GetList(MediaType type) const {
	return type == MediaType::kAnime ? animeEntries_ : mangaEntries_;
}

const std::vector<Entry>& Catalogue::GetSaved(MediaType type) const {
	return type == MediaType::kAnime ? savedAnime_ : savedManga_;
}

int Catalogue::AddEntry(Entry added) {
	auto& list = GetList(added.type);
	for (auto& existing : list) {
		if (not SameTitle(existing, added))
			continue;
		existing = MergeImported(existing, added);
		return existing.id;
	}
	added.id = NextId(list);
	list.push_back(std::move(added));
	return list.back().id;
}

const std::vector<Entry>& Catalogue::GetEntries(MediaType type) const {
	return GetList(type);
}

std::optional<Entry> Catalogue::GetById(MediaType type, int id) const {
	const auto& list = GetList(type);
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
	return found == list.end() ? std::nullopt : std::optional<Entry>(*found);
}

std::optional<Entry> Catalogue::FindByIds(MediaType type, int anilist_id, int mal_id,
                                          int kitsu_id) const {
	Entry wanted;
	wanted.anilist_id = anilist_id;
	wanted.mal_id = mal_id;
	wanted.kitsu_id = kitsu_id;
	const auto& list = GetList(type);
	auto found = std::find_if(list.begin(), list.end(),
	                          [&wanted](const Entry& e) { return SameTitle(e, wanted); });
	return found == list.end() ? std::nullopt : std::optional<Entry>(*found);
}

bool Catalogue::UpdateEntry(const Entry& changed) {
	auto& list = GetList(changed.type);
	auto found = std::find_if(list.begin(), list.end(),
	                          [&changed](const Entry& e) { return e.id == changed.id; });
	if (found == list.end())
		return false;
	*found = changed;
	return true;
}

bool Catalogue::RemoveEntry(MediaType type, int id) {
	auto& list = GetList(type);
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
	if (found == list.end())
		return false;
	list.erase(found);
	return true;
}

bool Catalogue::RestoreEntry(const Entry& back) {
	auto& list = GetList(back.type);
	if (FindById(list, back.id))
		return false;
	// In its place by id, the order the lists are read in.
	auto after =
	    std::find_if(list.begin(), list.end(), [&back](const Entry& e) { return e.id > back.id; });
	list.insert(after, back);
	return true;
}

Provider Catalogue::provider() const {
	return provider_;
}

void Catalogue::set_provider(Provider site) {
	provider_ = site;
}

// What the sites are still to be told.

std::vector<Pending> Catalogue::Outbox() {
	std::vector<Pending> waiting;
	// No lists yet: nothing waits, and nothing is made to say so.
	std::error_code ignored;
	if (not db_.IsOpen() and not std::filesystem::exists(StorePath(), ignored))
		return waiting;
	if (not OpenStore().has_value() or not HasTable(db_, "outbox"))
		return waiting;
	// An outbox no write of this version has brought up to date yet.
	bool current = HasColumn(db_, "outbox", "next_try");
	auto rows = db_.Prepare(
	    current ? "SELECT site, type, id, removed, site_id, fields, attempts, next_try FROM outbox"
	              " ORDER BY seq"
	            : "SELECT site, type, id, removed, site_id, 127, 0, 0 FROM outbox ORDER BY seq");
	while (rows.Step()) {
		auto site = ParseProvider(rows.TextAt(0));
		if (not site)
			continue;
		waiting.push_back({*site, rows.TextAt(1) == "manga" ? MediaType::kManga : MediaType::kAnime,
		                   static_cast<int>(rows.IntAt(2)), rows.IntAt(3) != 0,
		                   static_cast<int>(rows.IntAt(4)), static_cast<unsigned>(rows.IntAt(5)),
		                   static_cast<int>(rows.IntAt(6)), rows.IntAt(7)});
	}
	return waiting;
}

bool Catalogue::Queue(const std::vector<Pending>& more) {
	return Commit([this, &more] {
		if (not AddOutboxColumns(db_))
			return false;
		auto known = db_.Prepare(
		    "SELECT fields FROM outbox WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		auto update = db_.Prepare(
		    "UPDATE outbox SET fields = ?, site_id = ?, attempts = 0, next_try = 0"
		    " WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		auto insert = db_.Prepare(
		    "INSERT INTO outbox (site, type, id, removed, site_id, fields) VALUES (?, ?, ?, ?, ?, ?)");
		for (const auto& each : more) {
			std::string site = ProviderKey(each.site);
			std::string type = TypeKey(each.type);
			int removed = each.removed ? 1 : 0;
			known.Bind(site).Bind(type).Bind(each.id).Bind(removed);
			bool waiting = known.Step();
			unsigned fields = waiting
			                      ? MergeFields(static_cast<unsigned>(known.IntAt(0)), each.fields)
			                      : each.fields;
			known.Reset();
			bool done = waiting ? update.Bind(static_cast<long long>(fields))
			                          .Bind(each.site_id)
			                          .Bind(site)
			                          .Bind(type)
			                          .Bind(each.id)
			                          .Bind(removed)
			                          .Run()
			                    : insert.Bind(site)
			                          .Bind(type)
			                          .Bind(each.id)
			                          .Bind(removed)
			                          .Bind(each.site_id)
			                          .Bind(static_cast<long long>(fields))
			                          .Run();
			if (not done)
				return false;
		}
		return true;
	});
}

bool Catalogue::Deferred(const Pending& waiting, int attempts, long long next_try) {
	return Commit([this, &waiting, attempts, next_try] {
		if (not AddOutboxColumns(db_))
			return false;
		auto later = db_.Prepare(
		    "UPDATE outbox SET attempts = ?, next_try = ?"
		    " WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		later.Bind(attempts)
		    .Bind(next_try)
		    .Bind(ProviderKey(waiting.site))
		    .Bind(TypeKey(waiting.type));
		return later.Bind(waiting.id).Bind(waiting.removed ? 1 : 0).Run();
	});
}

bool Catalogue::Sent(const Pending& done) {
	return Commit([this, &done] {
		auto remove = db_.Prepare(
		    "DELETE FROM outbox WHERE site = ? AND type = ? AND id = ? AND removed = ?");
		remove.Bind(ProviderKey(done.site)).Bind(TypeKey(done.type)).Bind(done.id);
		return remove.Bind(done.removed ? 1 : 0).Run();
	});
}

// Whether a title is hidden on a site, as the person said. The row keeps
// the site's id, so that a ref Catalist-TUI gives to another title does not
// take this one's choice with it.

namespace {

const char* kTrackOptionTable =
    "CREATE TABLE IF NOT EXISTS track_option ("
    " site TEXT NOT NULL, type TEXT NOT NULL, id INTEGER NOT NULL, site_id INTEGER NOT NULL,"
    " hidden INTEGER NOT NULL, PRIMARY KEY (site, type, id))";

}  // namespace

std::optional<bool> Catalogue::HiddenOn(Provider site, MediaType type, int id, int site_id) {
	std::error_code ignored;
	if (not db_.IsOpen() and not std::filesystem::exists(StorePath(), ignored))
		return std::nullopt;
	if (not OpenStore().has_value() or not HasTable(db_, "track_option"))
		return std::nullopt;
	auto row = db_.Prepare(
	    "SELECT hidden FROM track_option WHERE site = ? AND type = ? AND id = ? AND site_id = ?");
	row.Bind(ProviderKey(site)).Bind(TypeKey(type)).Bind(id).Bind(site_id);
	return row.Step() ? std::optional<bool>(row.IntAt(0) != 0) : std::nullopt;
}

bool Catalogue::SetHidden(Provider site, MediaType type, int id, int site_id, bool hidden) {
	return Commit([&] {
		if (not db_.Run(kTrackOptionTable))
			return false;
		auto set = db_.Prepare("INSERT OR REPLACE INTO track_option VALUES (?, ?, ?, ?, ?)");
		set.Bind(ProviderKey(site)).Bind(TypeKey(type)).Bind(id).Bind(site_id);
		return set.Bind(hidden ? 1 : 0).Run();
	});
}

// Opening and setting up the store.

Done Catalogue::OpenStore() {
	if (db_.IsOpen())
		return Done{};
	std::error_code ignored;
	std::filesystem::create_directories(dataDir_, ignored);
	if (not db_.Open(StorePath()))
		return std::unexpected(
		    std::format("cannot open {}: {}", StorePath().string(), db_.LastError()));
	auto ready = SetUpStore();
	if (not ready.has_value())
		db_.Close();
	return ready;
}

// A new file gets its tables, in one transaction: two programs starting at
// the same moment cannot both set it up.
Done Catalogue::SetUpStore() {
	if (StoreVersion(db_) == kStoreVersion)
		return Done{};

	store::Transaction setup(db_);
	if (not setup.Active())
		return std::unexpected(
		    std::format("cannot write to {}: {}", StorePath().string(), db_.LastError()));
	int version = StoreVersion(db_);
	if (version == kStoreVersion)
		return Done{};
	if (version != 0)
		return std::unexpected(
		    std::format("{} was written by a newer Catalist", StorePath().string()));

	if (not db_.Run(kSchema))
		return std::unexpected(
		    std::format("cannot set up {}: {}", StorePath().string(), db_.LastError()));
	if (not setup.Commit())
		return std::unexpected(
		    std::format("cannot write to {}: {}", StorePath().string(), db_.LastError()));
	return Done{};
}

Done Catalogue::BeginWrite() {
	auto opened = OpenStore();
	if (not opened.has_value())
		return opened;
	write_.emplace(db_);
	if (write_->Active())
		return Done{};
	write_.reset();
	return std::unexpected(
	    std::format("{} is in use by another program ({})", StorePath().string(), db_.LastError()));
}

// Reading.

Result<bool> Catalogue::LoadAll() {
	std::error_code ignored;
	bool stored = db_.IsOpen() or std::filesystem::exists(StorePath(), ignored);
	if (not stored)
		return false;
	auto opened = OpenStore();
	if (not opened)
		return std::unexpected(opened.error());
	if (not ReadStore())
		return std::unexpected(
		    std::format("cannot read {}: {}", StorePath().string(), db_.LastError()));
	return true;
}

bool Catalogue::Peek() {
	std::error_code ignored;
	if (db_.IsOpen() or not std::filesystem::exists(StorePath(), ignored))
		return false;
	if (not db_.Open(StorePath(), store::OpenMode::kReadOnly, kPeekWaitMs))
		return false;
	bool read = StoreVersion(db_) == kStoreVersion and ReadStore();
	db_.Close();
	return read;
}

bool Catalogue::ReadStore() {
	std::vector<Entry> anime;
	std::vector<Entry> manga;
	auto rows = db_.Prepare(kSelectEntries);
	while (rows.Step()) {
		Entry row = ReadEntry(rows);
		(row.type == MediaType::kAnime ? anime : manga).push_back(std::move(row));
	}
	Provider site = Provider::kAniList;
	// Other keys (Catalist-TUI's `linked`) are another program's and are
	// left as they are.
	auto settings = db_.Prepare("SELECT key, value FROM setting");
	while (settings.Step()) {
		if (settings.TextAt(0) == kProviderKey)
			site = ParseProvider(settings.TextAt(1)).value_or(site);
	}
	// An error ends a statement the way its last row does.
	if (not rows.Ok() or not settings.Ok() or not ReadSiteIds(db_, anime, manga))
		return false;

	animeEntries_ = std::move(anime);
	mangaEntries_ = std::move(manga);
	provider_ = site;
	RememberSaved();
	return true;
}

void Catalogue::RememberSaved() {
	savedAnime_ = animeEntries_;
	savedManga_ = mangaEntries_;
	savedProvider_ = provider_;
}

// Writing.

bool Catalogue::Commit(const std::function<bool()>& work) {
	if (not OpenStore().has_value())
		return false;
	if (not write_)
		write_.emplace(db_);
	bool done = write_->Active() and work() and write_->Commit();
	// A write that did not get that far is rolled back here.
	write_.reset();
	return done;
}

bool Catalogue::WriteRows() {
	auto insert = db_.Prepare(kInsertEntry);
	auto remove = db_.Prepare("DELETE FROM entry WHERE type = ? AND id = ?");
	bool has_outbox = HasTable(db_, "outbox");
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		const auto& now = GetList(type);
		const auto& before = GetSaved(type);
		for (const auto& row : now) {
			const Entry* was = FindById(before, row.id);
			bool same = was and *was == row;
			if (not same and not BindEntry(insert, row).Run())
				return false;
		}
		for (const auto& row : before) {
			bool gone = FindById(now, row.id) == nullptr;
			if (not gone)
				continue;
			if (not remove.Bind(TypeKey(type)).Bind(row.id).Run())
				return false;
			if (has_outbox and
			    not db_.Prepare("DELETE FROM outbox WHERE type = ? AND id = ? AND removed = 0")
			            .Bind(TypeKey(type))
			            .Bind(row.id)
			            .Run())
				return false;
		}
	}
	return WriteSiteIds();
}

// The Kitsu ids, written anew: the rows of titles that are gone go too.
bool Catalogue::WriteSiteIds() {
	if (not db_.Run(kSiteIdTable) or not db_.Run("DELETE FROM entry_id WHERE site = 'kitsu'"))
		return false;
	auto insert = db_.Prepare("INSERT INTO entry_id VALUES (?, ?, 'kitsu', ?, ?, ?)");
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		for (const auto& row : GetList(type)) {
			if (row.kitsu_id <= 0)
				continue;
			insert.Bind(TypeKey(type)).Bind(row.id).Bind(row.kitsu_id);
			if (not insert.Bind(row.mal_id).Bind(row.anilist_id).Run())
				return false;
		}
	}
	return true;
}

bool Catalogue::WriteSettings() {
	auto set = db_.Prepare(kInsertSetting);
	// MyAnimeList as Catalist 0.3 wrote it; Catalist-TUI reads "kitsu" as
	// AniList, its default.
	std::string site = provider_ == Provider::kMyAnimeList ? "myanimelist" : ProviderKey(provider_);
	return set.Bind(std::string(kProviderKey)).Bind(site).Run();
}

bool Catalogue::SaveSettings() {
	if (not Commit([this] { return WriteSettings(); }))
		return false;
	savedProvider_ = provider_;
	return true;
}

bool Catalogue::WriteChanged() {
	bool settings_changed = provider_ != savedProvider_;
	return WriteRows() and (not settings_changed or WriteSettings());
}

bool Catalogue::SaveAll() {
	bool done = Commit([this] { return WriteChanged(); });
	if (done)
		RememberSaved();
	return done;
}

}  // namespace catalist
