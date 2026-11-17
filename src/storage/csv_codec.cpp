#include "storage/csv_codec.hpp"

#include <format>
#include <sstream>

namespace catalist::csv_codec {

namespace {

// The last three columns came later; rows without them are still read.
constexpr size_t kCatalogFields = 14;
// Catalist 0.2.1 to 0.3.0 wrote an AniList id after the MyAnimeList id and
// notes before the last date; the notes are skipped.
constexpr size_t kOldCatalogFields = 16;

std::optional<int> ParseInt(const std::string& field) {
	try {
		return std::stoi(field);
	} catch (...) {
		return std::nullopt;
	}
}

int IntOrZero(const std::string& field) {
	return ParseInt(field).value_or(0);
}

bool IsTypeColumn(const std::string& field) {
	return field == "anime" or field == "manga";
}

MediaType TypeOf(const std::string& field) {
	return field == "manga" ? MediaType::kManga : MediaType::kAnime;
}

bool TogglesQuoteState(const std::string& line, bool inside_quotes) {
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] != '"')
			continue;
		bool escaped_quote = inside_quotes and i + 1 < line.size() and line[i + 1] == '"';
		if (escaped_quote)
			++i;
		else
			inside_quotes = not inside_quotes;
	}
	return inside_quotes;
}

std::vector<std::string> SplitLines(std::string_view text) {
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

bool IsSkippableLine(const std::string& line) {
	return line.empty() or line[0] == '#';
}

// Our own files only hold names we wrote, so an unknown status is a hand
// edit; keeping the row (as Plan to Watch) beats dropping it on the next save.
WatchStatus DecodeStatus(const std::string& field) {
	return ParseStatus(field).value_or(WatchStatus::kPlanToWatch);
}

}  // namespace

std::string EscapeField(const std::string& field) {
	bool needs_quotes = field.find_first_of(",\"\n\r") != std::string::npos;
	if (not needs_quotes)
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

std::vector<std::string> SplitRecords(std::string_view text) {
	std::vector<std::string> records;
	std::string pending;
	bool inside_quotes = false;
	for (const auto& line : SplitLines(text)) {
		if (pending.empty() and IsSkippableLine(line))
			continue;
		inside_quotes = TogglesQuoteState(line, inside_quotes);
		pending += (pending.empty() ? "" : "\n") + line;
		if (inside_quotes)
			continue;
		records.push_back(std::move(pending));
		pending.clear();
	}
	return records;
}

std::vector<std::string> SplitFields(std::string_view record) {
	std::vector<std::string> fields;
	std::string field;
	bool in_quotes = false;
	for (size_t i = 0; i < record.size(); ++i) {
		char c = record[i];
		bool escaped_quote =
		    in_quotes and c == '"' and i + 1 < record.size() and record[i + 1] == '"';
		if (escaped_quote) {
			field += '"';
			++i;
		} else if (c == '"') {
			in_quotes = not in_quotes;
		} else if (c == ',' and not in_quotes) {
			fields.push_back(std::move(field));
			field.clear();
		} else {
			field += c;
		}
	}
	fields.push_back(std::move(field));
	return fields;
}

std::optional<IsoDate> DecodeDate(const std::string& field) {
	// Files written before dates became optional used "0000-00-00" for an
	// unknown date; parse rejects it (there is no year 0), so it and an empty
	// field both mean "no date".
	return IsoDate::Parse(field);
}

std::optional<Entry> DecodeEntry(const std::vector<std::string>& fields) {
	bool current = fields.size() >= kCatalogFields and IsTypeColumn(fields[2]);
	bool old = fields.size() >= kOldCatalogFields and IsTypeColumn(fields[3]);
	auto id = current or old ? ParseInt(fields[0]) : std::nullopt;
	if (not id)
		return std::nullopt;

	// Every column from the type on sits one further right in the old layout.
	size_t shift = old ? 1 : 0;
	auto field = [&](size_t index) -> const std::string& { return fields[index + shift]; };
	Entry decoded;
	decoded.type = TypeOf(field(2));
	decoded.id = *id;
	decoded.mal_id = IntOrZero(fields[1]);
	// The AniList id: third in the old layout, last in the current one.
	if (old)
		decoded.anilist_id = IntOrZero(fields[2]);
	else if (fields.size() > 16)
		decoded.anilist_id = IntOrZero(fields[16]);
	decoded.title = field(3);
	decoded.status = DecodeStatus(field(4));
	decoded.score = IntOrZero(field(5));
	decoded.progress = IntOrZero(field(6));
	decoded.total_units = IntOrZero(field(7));
	decoded.volumes = IntOrZero(field(8));
	decoded.total_volumes = IntOrZero(field(9));
	decoded.rewatched = IntOrZero(field(10));
	decoded.start_date = DecodeDate(field(11));
	decoded.finish_date = DecodeDate(field(12));
	decoded.updated_at = DecodeDate(fields[old ? 15 : 13]);
	if (current and fields.size() > 15) {
		decoded.released = IntOrZero(fields[14]);
		decoded.checked_at = DecodeDate(fields[15]);
	}
	// The Kitsu id, after the AniList id: only rows of the undo history have
	// it. Catalist-TUI reads such a row and leaves the field alone.
	if (current and fields.size() > 17)
		decoded.kitsu_id = IntOrZero(fields[17]);
	return decoded;
}

Result<std::vector<Entry>> DecodeEntries(std::string_view text) {
	std::vector<Entry> entries;
	for (const auto& record : SplitRecords(text)) {
		auto decoded = DecodeEntry(SplitFields(record));
		if (not decoded)
			return std::unexpected(std::format(
			    "row {} has a column layout this version cannot read", entries.size() + 1));
		entries.push_back(std::move(*decoded));
	}
	return entries;
}

std::string EncodeEntry(const Entry& e) {
	return std::format(
	    "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}{}", e.id, e.mal_id, TypeKey(e.type),
	    EscapeField(e.title), EscapeField(StatusToString(e.status, e.type)), e.score, e.progress,
	    e.total_units, e.volumes, e.total_volumes, e.rewatched, FormatDate(e.start_date),
	    FormatDate(e.finish_date), FormatDate(e.updated_at), e.released, FormatDate(e.checked_at),
	    e.anilist_id, e.kitsu_id > 0 ? std::format(",{}", e.kitsu_id) : "");
}

}  // namespace catalist::csv_codec
