#include "catalogue.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <utility>

#include "entry.hpp"
#include "system.hpp"

namespace catalist::csvCodec {

namespace {

const char* kCatalogHeader =
    "#id,mal_id,type,title,status,score,progress,total_units,volumes,"
    "total_volumes,rewatched,start_date,finish_date,updated_at,released,"
    "checked_at,anilist_id\n";

// The last three columns came later; rows without them are still read.
constexpr size_t kCatalogFields = 14;
// Catalist 0.2.1 to 0.3.0 wrote an AniList id after the MyAnimeList id and
// notes before the last date; the notes are skipped.
constexpr size_t kOldCatalogFields = 16;

std::optional<int> parseInt(const std::string& field) {
	try {
		return std::stoi(field);
	} catch (...) {
		return std::nullopt;
	}
}

int intOrZero(const std::string& field) {
	return parseInt(field).value_or(0);
}

bool isTypeColumn(const std::string& field) {
	return field == "anime" or field == "manga";
}

bool togglesQuoteState(const std::string& line, bool insideQuotes) {
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] != '"')
			continue;
		bool escapedQuote = insideQuotes and i + 1 < line.size() and line[i + 1] == '"';
		if (escapedQuote)
			++i;
		else
			insideQuotes = not insideQuotes;
	}
	return insideQuotes;
}

std::vector<std::string> splitLines(std::string_view text) {
	std::vector<std::string> lines;
	std::istringstream stream{std::string(text)};
	std::string line;
	while (std::getline(stream, line)) {
		if (not line.empty() and line.back() == '\r')
			line.pop_back();
		lines.push_back(line);
	}
	return lines;
}

bool isSkippableLine(const std::string& line) {
	return line.empty() or line[0] == '#';
}

// Our own files only hold names we wrote, so an unknown status is a hand
// edit; keeping the row (as Plan to Watch) beats dropping it on the next save.
watchStatus decodeStatus(const std::string& field) {
	return parseStatus(field).value_or(watchStatus::kPlanToWatch);
}

}  // namespace

std::string catalogFileName(mediaType type) {
	return type == mediaType::kAnime ? "anime.csv" : "manga.csv";
}

std::string escapeField(const std::string& field) {
	bool needsQuotes = field.find_first_of(",\"\n\r") != std::string::npos;
	if (not needsQuotes)
		return field;
	std::string escaped = "\"";
	for (char c : field) {
		if (c == '"')
			escaped += "\"\"";
		else
			escaped += c;
	}
	escaped += "\"";
	return escaped;
}

std::vector<std::string> splitRecords(std::string_view text) {
	std::vector<std::string> records;
	std::string pending;
	bool insideQuotes = false;
	for (const auto& line : splitLines(text)) {
		if (pending.empty() and isSkippableLine(line))
			continue;
		insideQuotes = togglesQuoteState(line, insideQuotes);
		pending += (pending.empty() ? "" : "\n") + line;
		if (insideQuotes)
			continue;
		records.push_back(std::move(pending));
		pending.clear();
	}
	return records;
}

std::vector<std::string> splitFields(std::string_view record) {
	std::vector<std::string> fields;
	std::string field;
	bool inQuotes = false;
	for (size_t i = 0; i < record.size(); ++i) {
		char c = record[i];
		bool escapedQuote =
		    inQuotes and c == '"' and i + 1 < record.size() and record[i + 1] == '"';
		if (escapedQuote) {
			field += '"';
			++i;
		} else if (c == '"') {
			inQuotes = not inQuotes;
		} else if (c == ',' and not inQuotes) {
			fields.push_back(std::move(field));
			field.clear();
		} else {
			field += c;
		}
	}
	fields.push_back(std::move(field));
	return fields;
}

std::optional<isoDate> decodeDate(const std::string& field) {
	// Files written before dates became optional used "0000-00-00" for an
	// unknown date; parse rejects it (there is no year 0), so it and an empty
	// field both mean "no date".
	return isoDate::parse(field);
}

std::optional<entry> decodeEntry(const std::vector<std::string>& fields, mediaType type) {
	bool current = fields.size() >= kCatalogFields and isTypeColumn(fields[2]);
	bool old = fields.size() >= kOldCatalogFields and isTypeColumn(fields[3]);
	auto id = current or old ? parseInt(fields[0]) : std::nullopt;
	if (not id)
		return std::nullopt;

	// Every column from the type on sits one further right in the old layout.
	size_t shift = old ? 1 : 0;
	auto field = [&](size_t index) -> const std::string& { return fields[index + shift]; };
	entry decoded;
	decoded.type = type;
	decoded.id = *id;
	decoded.malId = intOrZero(fields[1]);
	// The AniList id: third in the old layout, last in the current one.
	if (old)
		decoded.anilistId = intOrZero(fields[2]);
	else if (fields.size() > 16)
		decoded.anilistId = intOrZero(fields[16]);
	decoded.title = field(3);
	decoded.status = decodeStatus(field(4));
	decoded.score = intOrZero(field(5));
	decoded.progress = intOrZero(field(6));
	decoded.totalUnits = intOrZero(field(7));
	decoded.volumes = intOrZero(field(8));
	decoded.totalVolumes = intOrZero(field(9));
	decoded.rewatched = intOrZero(field(10));
	decoded.startDate = decodeDate(field(11));
	decoded.finishDate = decodeDate(field(12));
	decoded.updatedAt = decodeDate(fields[old ? 15 : 13]);
	if (current and fields.size() > 15) {
		decoded.released = intOrZero(fields[14]);
		decoded.checkedAt = decodeDate(fields[15]);
	}
	return decoded;
}

result<std::vector<entry>> decodeEntries(std::string_view text, mediaType type) {
	std::vector<entry> entries;
	for (const auto& record : splitRecords(text)) {
		auto decoded = decodeEntry(splitFields(record), type);
		if (not decoded)
			return result<std::vector<entry>>::failure(fmt::format(
			    "row {} has a column layout this version cannot read", entries.size() + 1));
		entries.push_back(std::move(*decoded));
	}
	return result<std::vector<entry>>::success(std::move(entries));
}

std::string encodeEntry(const entry& e) {
	return fmt::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}", e.id, e.malId,
	                   typeKey(e.type), escapeField(e.title),
	                   escapeField(statusToString(e.status, e.type)), e.score, e.progress,
	                   e.totalUnits, e.volumes, e.totalVolumes, e.rewatched,
	                   formatDate(e.startDate), formatDate(e.finishDate), formatDate(e.updatedAt),
	                   e.released, formatDate(e.checkedAt), e.anilistId);
}

std::string encodeEntries(const std::vector<entry>& entries) {
	std::string text = kCatalogHeader;
	for (const auto& e : entries)
		text += encodeEntry(e) + "\n";
	return text;
}

}  // namespace catalist::csvCodec

namespace catalist {

namespace {

const char* kStoreFile = "catalist.db";
// What Catalist 0.3 and earlier kept beside its two CSV files.
const char* kSettingsFile = "settings";
const char* kProviderKey = "provider";
const char* kLinkedKey = "linked";

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

// ---- The undo history ----

std::optional<entry> decodeRow(const std::string& row, mediaType type) {
	return row.empty() ? std::nullopt : csvCodec::decodeEntry(csvCodec::splitFields(row), type);
}

std::string encodeRow(const std::optional<entry>& row) {
	return row ? csvCodec::encodeEntry(*row) : std::string();
}

// The changes of one step in the order they were made; nothing when they
// cannot be read.
std::optional<std::vector<journal::change>> readChanges(const store::database& db, long long tx) {
	std::vector<journal::change> changes;
	auto rows = db.prepare("SELECT type, id, before, after FROM change WHERE tx = ? ORDER BY seq");
	rows.bind(tx);
	while (rows.step()) {
		mediaType type = rows.textAt(0) == "manga" ? mediaType::kManga : mediaType::kAnime;
		changes.push_back({type, static_cast<int>(rows.intAt(1)), decodeRow(rows.textAt(2), type),
		                   decodeRow(rows.textAt(3), type)});
	}
	return rows.ok() ? std::optional(changes) : std::nullopt;
}

bool writeChanges(const store::database& db, long long tx,
                  const std::vector<journal::change>& changes) {
	auto insert = db.prepare("INSERT INTO change VALUES (?, ?, ?, ?, ?, ?)");
	int seq = 0;
	for (const auto& made : changes) {
		insert.bind(tx).bind(seq++).bind(typeKey(made.type)).bind(made.id);
		if (not insert.bind(encodeRow(made.before)).bind(encodeRow(made.after)).run())
			return false;
	}
	return true;
}

// The steps that `where` selects and orders (all of them when `limit` is
// not positive); nothing when they cannot be read.
std::optional<std::vector<txRecord>> readSteps(const store::database& db, const std::string& where,
                                               int limit) {
	std::vector<txRecord> steps;
	auto rows = db.prepare(fmt::format(
	    "SELECT id, stamp, label, undone,"
	    " (SELECT COUNT(*) FROM change WHERE change.tx = tx.id) FROM tx WHERE {} LIMIT ?",
	    where));
	rows.bind(limit > 0 ? limit : -1);
	while (rows.step())
		steps.push_back({rows.intAt(0), rows.textAt(1), rows.textAt(2), rows.intAt(3) != 0,
		                 static_cast<int>(rows.intAt(4))});
	return rows.ok() ? std::optional(steps) : std::nullopt;
}

// The newest step when it has that label, else 0.
long long newestStepLabelled(const store::database& db, const std::string& label) {
	auto newest = db.prepare("SELECT id, label FROM tx ORDER BY id DESC LIMIT 1");
	return newest.step() and newest.textAt(1) == label ? newest.intAt(0) : 0;
}

bool runWith(const store::database& db, const std::string& sql, long long tx) {
	auto statement = db.prepare(sql);
	return statement.bind(tx).run();
}

// Enters the changes as a step, or folds them into the newest one when the
// note says so and that one has its label.
bool writeJournal(store::database& db, const txNote& note,
                  const std::vector<journal::change>& changes) {
	// New work ends what could be redone.
	if (not db.run("DELETE FROM change WHERE tx IN (SELECT id FROM tx WHERE undone = 1);"
	               "DELETE FROM tx WHERE undone = 1;"))
		return false;

	long long step = note.group ? newestStepLabelled(db, note.label) : 0;
	if (step > 0) {
		auto earlier = readChanges(db, step);
		if (not earlier or not runWith(db, "DELETE FROM change WHERE tx = ?", step))
			return false;
		auto all = journal::merge(*earlier, changes);
		// A group that has put everything back is no step at all.
		return all.empty() ? runWith(db, "DELETE FROM tx WHERE id = ?", step)
		                   : writeChanges(db, step, all);
	}
	auto insert = db.prepare("INSERT INTO tx (stamp, label) VALUES (?, ?)");
	if (not insert.bind(note.stamp).bind(note.label).run() or
	    not writeChanges(db, db.lastInsertId(), changes))
		return false;
	return db.run(
	    fmt::format("DELETE FROM tx WHERE id NOT IN (SELECT id FROM tx ORDER BY id DESC LIMIT {});"
	                "DELETE FROM change WHERE tx NOT IN (SELECT id FROM tx);",
	                catalogue::kHistoryLength));
}

const entry* findById(const std::vector<entry>& list, int id) {
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const entry& e) { return e.id == id; });
	return found == list.end() ? nullptr : &*found;
}

bool hasOldFiles(const std::filesystem::path& dir) {
	std::error_code ignored;
	return std::filesystem::is_regular_file(dir / "anime.csv", ignored) or
	       std::filesystem::is_regular_file(dir / "manga.csv", ignored) or
	       std::filesystem::is_regular_file(dir / kSettingsFile, ignored);
}

std::vector<std::pair<std::string, std::string>> readOldSettings(
    const std::filesystem::path& file) {
	auto trim = [](std::string text) {
		text.erase(0, text.find_first_not_of(" \t"));
		text.erase(text.find_last_not_of(" \t\r") + 1);
		return text;
	};
	std::vector<std::pair<std::string, std::string>> settings;
	std::istringstream lines(readFile(file).value_or(""));
	std::string line;
	while (std::getline(lines, line)) {
		auto equals = line.find('=');
		if (equals != std::string::npos)
			settings.emplace_back(trim(line.substr(0, equals)), trim(line.substr(equals + 1)));
	}
	return settings;
}

// A local id only numbers a title, so two titles are the same only when a
// site's id says so.
bool sameTitle(const entry& existing, const entry& incoming) {
	return (incoming.anilistId > 0 and existing.anilistId == incoming.anilistId) or
	       (incoming.malId > 0 and existing.malId == incoming.malId);
}

int nextId(const std::vector<entry>& list) {
	auto highest = std::max_element(list.begin(), list.end(),
	                                [](const entry& a, const entry& b) { return a.id < b.id; });
	return highest == list.end() ? 1 : highest->id + 1;
}

// The sum of one count over a list, e.g. sumOf(list, &entry::progress).
int sumOf(const std::vector<entry>& list, int entry::* count) {
	int total = 0;
	for (const auto& e : list)
		total += e.*count;
	return total;
}

// True when `a` comes before `b` in the natural order of `key`; ties fall
// back to the title so the order never depends on how the list was stored.
bool comesBefore(const entry& a, const entry& b, sortKey key) {
	switch (key) {
		case sortKey::kNew:
			if (newReleases(a) != newReleases(b))
				return newReleases(a) > newReleases(b);
			[[fallthrough]];
		case sortKey::kUpdated:
			if (a.updatedAt != b.updatedAt)
				return a.updatedAt > b.updatedAt;
			break;
		case sortKey::kScore:
			if (a.score != b.score)
				return a.score > b.score;
			break;
		case sortKey::kProgress:
			if (a.progress != b.progress)
				return a.progress > b.progress;
			break;
		case sortKey::kDate:
			if (listDate(a) != listDate(b))
				return listDate(a) > listDate(b);
			break;
		case sortKey::kStatus:
			if (a.status != b.status)
				return static_cast<int>(a.status) < static_cast<int>(b.status);
			break;
		case sortKey::kTitle:
			break;
	}
	return a.title < b.title;
}

}  // namespace

std::filesystem::path catalogue::getDefaultDataDir(const dataDirEnv& env) {
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

std::filesystem::path catalogue::getOldDataDir(const dataDirEnv& env) {
	if (not env.catalistDataDir.empty())
		return {};
	if (not env.xdgConfigHome.empty())
		return std::filesystem::path(env.xdgConfigHome) / "catalist";
	if (not env.home.empty())
		return std::filesystem::path(env.home) / ".config" / "catalist";
	return {};
}

catalogue::catalogue(std::filesystem::path dataDir, std::filesystem::path oldDir)
    : dataDir_(std::move(dataDir)), oldDir_(std::move(oldDir)) {}

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

void catalogue::sortEntries(std::vector<entry>& entries, sortKey key, bool reverse) {
	// Stable, so titles that compare equal keep the order they came in and
	// the same command prints the same lines every time.
	std::stable_sort(entries.begin(), entries.end(),
	                 [key, reverse](const entry& a, const entry& b) {
		                 return reverse ? comesBefore(b, a, key) : comesBefore(a, b, key);
	                 });
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

std::optional<entry> catalogue::findByIds(mediaType type, int anilistId, int malId) const {
	entry wanted;
	wanted.anilistId = anilistId;
	wanted.malId = malId;
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

size_t catalogue::countByStatus(mediaType type, watchStatus status) const {
	const auto& list = getList(type);
	return static_cast<size_t>(std::count_if(
	    list.begin(), list.end(), [status](const entry& e) { return e.status == status; }));
}

size_t catalogue::countByScore(mediaType type, int score) const {
	const auto& list = getList(type);
	return static_cast<size_t>(std::count_if(list.begin(), list.end(),
	                                         [score](const entry& e) { return e.score == score; }));
}

double catalogue::averageScore(mediaType type) const {
	int sum = 0;
	int rated = 0;
	for (const auto& e : getList(type)) {
		if (e.score > 0) {
			sum += e.score;
			++rated;
		}
	}
	return rated == 0 ? 0.0 : static_cast<double>(sum) / rated;
}

int catalogue::totalProgress(mediaType type) const {
	return sumOf(getList(type), &entry::progress);
}

int catalogue::totalVolumes(mediaType type) const {
	return sumOf(getList(type), &entry::volumes);
}

int catalogue::totalRewatched(mediaType type) const {
	return sumOf(getList(type), &entry::rewatched);
}

double catalogue::daysSpent(mediaType type) const {
	constexpr double kMinutesPerDay = 1440.0;
	double minutesPerUnit = type == mediaType::kAnime ? 24.0 : 5.0;
	return totalProgress(type) * minutesPerUnit / kMinutesPerDay;
}

provider catalogue::getProvider() const {
	return provider_;
}

void catalogue::setProvider(provider site) {
	provider_ = site;
}

std::optional<isoDate> catalogue::getLinkedOn() const {
	return linkedOn_;
}

void catalogue::setLinkedOn(std::optional<isoDate> day) {
	linkedOn_ = day;
}

int catalogue::fillIds(mediaType type, const std::vector<std::pair<int, int>>& anilistAndMal) {
	int filled = 0;
	for (auto& e : getList(type)) {
		for (const auto& [anilistId, malId] : anilistAndMal) {
			bool same =
			    (anilistId > 0 and e.anilistId == anilistId) or (malId > 0 and e.malId == malId);
			if (not same)
				continue;
			bool gains = (e.anilistId == 0 and anilistId > 0) or (e.malId == 0 and malId > 0);
			if (e.anilistId == 0)
				e.anilistId = anilistId;
			if (e.malId == 0)
				e.malId = malId;
			filled += gains ? 1 : 0;
			break;
		}
	}
	return filled;
}

// Opening and setting up the store.

result<bool> catalogue::openStore() {
	if (db_.isOpen())
		return result<bool>::success(true);
	std::error_code ignored;
	std::filesystem::create_directories(dataDir_, ignored);
	if (not db_.open(getStorePath()))
		return result<bool>::failure(
		    fmt::format("cannot open {}: {}", getStorePath().string(), db_.lastError()));
	auto ready = setUpStore();
	if (not ready.ok())
		db_.close();
	return ready;
}

// A new file gets its tables and, once, the lists of the CSV files that came
// before it. All of that is one transaction, so two programs starting at
// the same moment cannot both import.
result<bool> catalogue::setUpStore() {
	if (storeVersion(db_) == kStoreVersion)
		return result<bool>::success(true);

	store::transaction setup(db_);
	if (not setup.active())
		return result<bool>::failure(
		    fmt::format("cannot write to {}: {}", getStorePath().string(), db_.lastError()));
	int version = storeVersion(db_);
	if (version == kStoreVersion)
		return result<bool>::success(true);
	if (version != 0)
		return result<bool>::failure(
		    fmt::format("{} was written by a newer Catalist", getStorePath().string()));

	if (not db_.run(kSchema))
		return result<bool>::failure(
		    fmt::format("cannot set up {}: {}", getStorePath().string(), db_.lastError()));
	auto from = importSource();
	if (not from.empty()) {
		auto imported = importOldFiles(from);
		if (not imported.ok())
			return imported;
	}
	if (not setup.commit())
		return result<bool>::failure(
		    fmt::format("cannot write to {}: {}", getStorePath().string(), db_.lastError()));
	importedFrom_ = from;
	return result<bool>::success(true);
}

// The directory whose CSV files are imported into a new store: the data
// directory when it has any, else the old one; empty when neither has.
std::filesystem::path catalogue::importSource() const {
	if (hasOldFiles(dataDir_))
		return dataDir_;
	if (not oldDir_.empty() and hasOldFiles(oldDir_))
		return oldDir_;
	return {};
}

result<bool> catalogue::importOldFiles(const std::filesystem::path& from) {
	std::vector<entry> rows;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		auto file = from / csvCodec::catalogFileName(type);
		auto content = readFile(file);
		if (not content)
			continue;
		auto decoded = csvCodec::decodeEntries(*content, type);
		if (not decoded.ok())
			return result<bool>::failure(
			    fmt::format("{}: {}. Open and save it once with Catalist 0.3.0 to "
			                "convert it.",
			                file.string(), decoded.errorMessage()));
		rows.insert(rows.end(), decoded.value().begin(), decoded.value().end());
	}
	auto insert = db_.prepare(kInsertEntry);
	for (const auto& row : rows) {
		if (not bindEntry(insert, row).run())
			return result<bool>::failure(
			    fmt::format("cannot write to {}: {}", getStorePath().string(), db_.lastError()));
	}

	// The settings file: `key = value` lines; unknown keys and values are
	// ignored.
	auto set = db_.prepare(kInsertSetting);
	for (const auto& [key, value] : readOldSettings(from / kSettingsFile)) {
		bool known = (key == kProviderKey and parseProvider(value)) or
		             (key == kLinkedKey and isoDate::parse(value));
		if (known and not set.bind(key).bind(value).run())
			return result<bool>::failure(
			    fmt::format("cannot write to {}: {}", getStorePath().string(), db_.lastError()));
	}
	return result<bool>::success(true);
}

result<bool> catalogue::beginWrite() {
	auto opened = openStore();
	if (not opened.ok())
		return opened;
	write_.emplace(db_);
	if (write_->active())
		return result<bool>::success(true);
	write_.reset();
	return result<bool>::failure(fmt::format("{} is in use by another program ({})",
	                                         getStorePath().string(), db_.lastError()));
}

// Reading.

result<bool> catalogue::loadAll() {
	std::error_code ignored;
	bool stored = db_.isOpen() or std::filesystem::exists(getStorePath(), ignored);
	if (not stored and importSource().empty())
		return result<bool>::success(false);
	auto opened = openStore();
	if (not opened.ok())
		return opened;
	if (not readStore())
		return result<bool>::failure(
		    fmt::format("cannot read {}: {}", getStorePath().string(), db_.lastError()));
	return result<bool>::success(true);
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
	std::optional<isoDate> linked;
	auto settings = db_.prepare("SELECT key, value FROM setting");
	while (settings.step()) {
		std::string key = settings.textAt(0);
		if (key == kProviderKey)
			site = parseProvider(settings.textAt(1)).value_or(site);
		if (key == kLinkedKey)
			linked = isoDate::parse(settings.textAt(1));
	}
	// An error ends a statement the way its last row does.
	if (not rows.ok() or not settings.ok())
		return false;

	animeEntries_ = std::move(anime);
	mangaEntries_ = std::move(manga);
	provider_ = site;
	linkedOn_ = linked;
	rememberSaved();
	return true;
}

void catalogue::rememberSaved() {
	savedAnime_ = animeEntries_;
	savedManga_ = mangaEntries_;
	savedProvider_ = provider_;
	savedLinkedOn_ = linkedOn_;
}

// Writing.

template <typename writer>
bool catalogue::commit(writer work) {
	if (not openStore().ok())
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
			bool same = was and csvCodec::encodeEntry(*was) == csvCodec::encodeEntry(row);
			if (not same and not bindEntry(insert, row).run())
				return false;
		}
		for (const auto& row : before) {
			bool gone = findById(now, row.id) == nullptr;
			if (gone and not remove.bind(typeKey(type)).bind(row.id).run())
				return false;
		}
	}
	return true;
}

bool catalogue::writeSettings() {
	auto set = db_.prepare(kInsertSetting);
	auto unset = db_.prepare("DELETE FROM setting WHERE key = ?");
	std::string site = provider_ == provider::kAniList ? "anilist" : "myanimelist";
	if (not set.bind(std::string(kProviderKey)).bind(site).run())
		return false;
	if (linkedOn_)
		return set.bind(std::string(kLinkedKey)).bind(linkedOn_->toString()).run();
	return unset.bind(std::string(kLinkedKey)).run();
}

bool catalogue::saveSettings() {
	if (not commit([this] { return writeSettings(); }))
		return false;
	savedProvider_ = provider_;
	savedLinkedOn_ = linkedOn_;
	return true;
}

bool catalogue::writeChanged() {
	bool settingsChanged = provider_ != savedProvider_ or linkedOn_ != savedLinkedOn_;
	return writeRows() and (not settingsChanged or writeSettings());
}

bool catalogue::saveAll() {
	bool done = commit([this] { return writeChanged(); });
	if (done)
		rememberSaved();
	return done;
}

bool catalogue::saveAll(const txNote& note) {
	auto changes = journal::diff(savedAnime_, animeEntries_);
	auto manga = journal::diff(savedManga_, mangaEntries_);
	changes.insert(changes.end(), manga.begin(), manga.end());
	if (changes.empty())
		return saveAll();
	bool done = commit([&] { return writeChanged() and writeJournal(db_, note, changes); });
	if (done)
		rememberSaved();
	return done;
}

// Undo and redo.

result<replay> catalogue::undo(int count, bool force) {
	return walkHistory(false, count, force);
}

result<replay> catalogue::redo(int count, bool force) {
	return walkHistory(true, count, force);
}

std::string catalogue::replayStep(const txRecord& step, bool redoing, bool force) {
	auto changes = readChanges(db_, step.id);
	if (not changes)
		return fmt::format("cannot read {}: {}", getStorePath().string(), db_.lastError());
	// What was done last is taken back first.
	if (not redoing)
		std::reverse(changes->begin(), changes->end());
	for (const auto& made : *changes) {
		const auto& from = redoing ? made.before : made.after;
		const auto& to = redoing ? made.after : made.before;
		auto next = journal::transition(getList(made.type), from, to, force);
		if (not next.ok())
			return fmt::format("cannot {} '{}' ({}): {}", redoing ? "redo" : "undo", step.label,
			                   step.stamp, next.errorMessage());
		if (not next.value())
			removeEntry(made.type, made.id);
		else if (not updateEntry(*next.value()))
			restoreEntry(*next.value());
	}
	return "";
}

result<replay> catalogue::walkHistory(bool redoing, int count, bool force) {
	using outcome = result<replay>;
	auto opened = openStore();
	if (not opened.ok())
		return outcome::failure(opened.errorMessage());
	auto steps = readSteps(
	    db_, redoing ? "undone = 1 ORDER BY id ASC" : "undone = 0 ORDER BY id DESC", count);
	if (not steps)
		return outcome::failure(
		    fmt::format("cannot read {}: {}", getStorePath().string(), db_.lastError()));
	if (steps->empty())
		return outcome::success({});

	// The lists as they are, to go back to when a step cannot be taken.
	std::vector<entry> anime = animeEntries_;
	std::vector<entry> manga = mangaEntries_;
	std::string problem;
	for (size_t i = 0; i < steps->size() and problem.empty(); ++i)
		problem = replayStep((*steps)[i], redoing, force);

	auto mark = [&] {
		auto set = db_.prepare("UPDATE tx SET undone = ? WHERE id = ?");
		return std::all_of(steps->begin(), steps->end(), [&](const txRecord& step) {
			return set.bind(redoing ? 0 : 1).bind(step.id).run();
		});
	};
	if (problem.empty() and not commit([&] { return writeChanged() and mark(); }))
		problem = fmt::format("could not save the catalogue to {}", getDataDir().string());
	if (not problem.empty()) {
		animeEntries_ = std::move(anime);
		mangaEntries_ = std::move(manga);
		return outcome::failure(problem);
	}

	replay done{*steps, journal::diff(anime, animeEntries_)};
	auto more = journal::diff(manga, mangaEntries_);
	done.changes.insert(done.changes.end(), more.begin(), more.end());
	rememberSaved();
	return outcome::success(std::move(done));
}

result<std::vector<txRecord>> catalogue::history(int limit) {
	using outcome = result<std::vector<txRecord>>;
	if (not db_.isOpen())
		return outcome::success({});
	auto steps = readSteps(db_, "1 ORDER BY id DESC", limit);
	if (not steps)
		return outcome::failure(
		    fmt::format("cannot read {}: {}", getStorePath().string(), db_.lastError()));
	return outcome::success(std::move(*steps));
}

}  // namespace catalist
