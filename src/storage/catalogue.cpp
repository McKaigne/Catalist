#include "storage/catalogue.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "core/entry.hpp"
#include "core/text.hpp"

namespace catalist {

namespace {

const char* kStoreFile = "catalist.db";

// PRAGMA user_version of a store this code set up; 0 is a new, empty file.
constexpr int kStoreVersion = 2;

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
    " updated_at TEXT, anilist_id INTEGER NOT NULL, PRIMARY KEY (type, id));"
    "CREATE TABLE tx ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT, stamp TEXT NOT NULL,"
    " label TEXT NOT NULL, undone INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE change ("
    " tx INTEGER NOT NULL, seq INTEGER NOT NULL, type TEXT NOT NULL,"
    " id INTEGER NOT NULL, before TEXT NOT NULL, after TEXT NOT NULL,"
    " PRIMARY KEY (tx, seq));"
    "PRAGMA user_version = 2;";

// From version 1, which also kept what the sites were to be told and how far
// each title had aired. Its undo rows have those columns too, so they go.
const char* kUpgradeFromV1 =
    "ALTER TABLE entry DROP COLUMN released;"
    "ALTER TABLE entry DROP COLUMN checked_at;"
    "DROP TABLE IF EXISTS entry_id;"
    "DROP TABLE IF EXISTS outbox;"
    "DROP TABLE IF EXISTS track_option;"
    "DROP TABLE IF EXISTS setting;"
    "DELETE FROM change;"
    "DELETE FROM tx;"
    "PRAGMA user_version = 2;";

const char* kSelectEntries =
    "SELECT type, id, mal_id, title, status, score, progress, total_units,"
    " volumes, total_volumes, rewatched, start_date, finish_date, updated_at,"
    " anilist_id FROM entry ORDER BY type, id";
const char* kInsertEntry =
    "INSERT OR REPLACE INTO entry VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

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
	row.anilist_id = number(14);
	return row;
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
	       (incoming.mal_id > 0 and existing.mal_id == incoming.mal_id);
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

int Catalogue::ImportEntry(Entry incoming) {
	if (incoming.anilist_id > 0 or incoming.mal_id > 0)
		return AddEntry(std::move(incoming));
	std::string name = utf8::Lowercase(incoming.title);
	for (auto& existing : GetList(incoming.type)) {
		bool has_id = existing.anilist_id > 0 or existing.mal_id > 0;
		if (has_id or utf8::Lowercase(existing.title) != name)
			continue;
		existing = MergeImported(existing, incoming);
		return existing.id;
	}
	return AddEntry(std::move(incoming));
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

std::optional<Entry> Catalogue::FindByIds(MediaType type, int anilist_id, int mal_id) const {
	Entry wanted;
	wanted.anilist_id = anilist_id;
	wanted.mal_id = mal_id;
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

// A new file gets its tables and an older one is brought up to date, in one
// transaction: two programs starting at the same moment cannot both do it.
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
	if (version != 0 and version != 1)
		return std::unexpected(
		    std::format("{} was written by a newer Catalist", StorePath().string()));

	if (not db_.Run(version == 0 ? kSchema : kUpgradeFromV1))
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

bool Catalogue::ReadStore() {
	std::vector<Entry> anime;
	std::vector<Entry> manga;
	auto rows = db_.Prepare(kSelectEntries);
	while (rows.Step()) {
		Entry row = ReadEntry(rows);
		(row.type == MediaType::kAnime ? anime : manga).push_back(std::move(row));
	}
	// An error ends a statement the way its last row does.
	if (not rows.Ok())
		return false;

	animeEntries_ = std::move(anime);
	mangaEntries_ = std::move(manga);
	RememberSaved();
	return true;
}

void Catalogue::RememberSaved() {
	savedAnime_ = animeEntries_;
	savedManga_ = mangaEntries_;
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
		}
	}
	return true;
}

bool Catalogue::SaveAll() {
	bool done = Commit([this] { return WriteRows(); });
	if (done)
		RememberSaved();
	return done;
}

}  // namespace catalist
