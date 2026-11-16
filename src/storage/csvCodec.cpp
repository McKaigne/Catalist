#include "storage/csvCodec.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>

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
