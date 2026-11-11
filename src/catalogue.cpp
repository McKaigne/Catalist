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
    "checked_at\n";

// The last two columns came later; rows without them are still read.
constexpr size_t kCatalogFields = 14;
// Catalist 0.2.1 to 0.3.0 wrote an AniList id after the MyAnimeList id and
// notes before the last date; those columns are skipped.
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

std::string typeKey(mediaType type) {
	return type == mediaType::kAnime ? "anime" : "manga";
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

std::string encodeEntries(const std::vector<entry>& entries) {
	std::ostringstream out;
	out << kCatalogHeader;
	for (const auto& e : entries) {
		out << e.id << "," << e.malId << "," << typeKey(e.type) << "," << escapeField(e.title)
		    << "," << escapeField(statusToString(e.status, e.type)) << "," << e.score << ","
		    << e.progress << "," << e.totalUnits << "," << e.volumes << "," << e.totalVolumes << ","
		    << e.rewatched << "," << formatDate(e.startDate) << "," << formatDate(e.finishDate)
		    << "," << formatDate(e.updatedAt) << "," << e.released << "," << formatDate(e.checkedAt)
		    << "\n";
	}
	return out.str();
}

}  // namespace catalist::csvCodec

namespace catalist {

namespace {

// A local id only numbers a title, so two titles are the same only when
// MyAnimeList says so.
bool sameMalTitle(const entry& existing, const entry& incoming) {
	return incoming.malId > 0 and existing.malId == incoming.malId;
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

std::filesystem::path catalogue::getDefaultConfigDir() {
	const char* custom = std::getenv("CATALIST_DATA_DIR");
	if (custom and *custom)
		return std::filesystem::path(custom);

	const char* xdg = std::getenv("XDG_CONFIG_HOME");
	if (xdg and *xdg)
		return std::filesystem::path(xdg) / "catalist";

	const char* home = std::getenv("HOME");
	if (home and *home)
		return std::filesystem::path(home) / ".config" / "catalist";

	return std::filesystem::path("catalist-data");
}

catalogue::catalogue(std::filesystem::path dataDir) : dataDir_(std::move(dataDir)) {}

std::filesystem::path catalogue::getConfigPath() const {
	return dataDir_;
}

std::filesystem::path catalogue::getCsvPath(mediaType type) const {
	return dataDir_ / csvCodec::catalogFileName(type);
}

std::vector<entry>& catalogue::getList(mediaType type) {
	return type == mediaType::kAnime ? animeEntries_ : mangaEntries_;
}

const std::vector<entry>& catalogue::getList(mediaType type) const {
	return type == mediaType::kAnime ? animeEntries_ : mangaEntries_;
}

void catalogue::sortEntries(std::vector<entry>& entries, sortKey key, bool reverse) {
	std::sort(entries.begin(), entries.end(), [key, reverse](const entry& a, const entry& b) {
		return reverse ? comesBefore(b, a, key) : comesBefore(a, b, key);
	});
}

int catalogue::addEntry(entry added) {
	auto& list = getList(added.type);
	for (auto& existing : list) {
		if (not sameMalTitle(existing, added))
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

std::optional<entry> catalogue::findByMalId(mediaType type, int malId) const {
	const auto& list = getList(type);
	auto found = std::find_if(list.begin(), list.end(),
	                          [malId](const entry& e) { return malId > 0 and e.malId == malId; });
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

result<bool> catalogue::loadCsv(mediaType type, const std::filesystem::path& path) {
	auto content = readFile(path);
	if (not content)
		return result<bool>::success(false);
	auto decoded = csvCodec::decodeEntries(*content, type);
	if (not decoded.ok())
		return result<bool>::failure(
		    fmt::format("{}: {}. Open and save it once with Catalist 0.3.0 to "
		                "convert it.",
		                path.string(), decoded.errorMessage()));
	getList(type) = std::move(decoded.value());
	return result<bool>::success(true);
}

bool catalogue::saveCsv(mediaType type, const std::filesystem::path& path) const {
	return writeFileAtomic(path, csvCodec::encodeEntries(getList(type)));
}

result<bool> catalogue::loadAll() {
	auto anime = loadCsv(mediaType::kAnime, getCsvPath(mediaType::kAnime));
	if (not anime.ok())
		return anime;
	auto manga = loadCsv(mediaType::kManga, getCsvPath(mediaType::kManga));
	if (not manga.ok())
		return manga;
	return result<bool>::success(anime.value() or manga.value());
}

bool catalogue::saveAll() const {
	bool anime = saveCsv(mediaType::kAnime, getCsvPath(mediaType::kAnime));
	bool manga = saveCsv(mediaType::kManga, getCsvPath(mediaType::kManga));
	return anime and manga;
}

}  // namespace catalist
