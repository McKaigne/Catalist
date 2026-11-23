#include "storage/csv_codec.hpp"

#include <format>

namespace catalist::csv_codec {

namespace {

constexpr size_t kCatalogFields = 15;

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

// The history only holds names we wrote; an unknown one reads as planned.
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

std::optional<Entry> DecodeEntry(const std::vector<std::string>& fields) {
	if (fields.size() < kCatalogFields or not IsTypeColumn(fields[2]))
		return std::nullopt;
	auto id = ParseInt(fields[0]);
	if (not id)
		return std::nullopt;
	Entry decoded;
	decoded.id = *id;
	decoded.mal_id = IntOrZero(fields[1]);
	decoded.type = TypeOf(fields[2]);
	decoded.title = fields[3];
	decoded.status = DecodeStatus(fields[4]);
	decoded.score = IntOrZero(fields[5]);
	decoded.progress = IntOrZero(fields[6]);
	decoded.total_units = IntOrZero(fields[7]);
	decoded.volumes = IntOrZero(fields[8]);
	decoded.total_volumes = IntOrZero(fields[9]);
	decoded.rewatched = IntOrZero(fields[10]);
	decoded.start_date = IsoDate::Parse(fields[11]);
	decoded.finish_date = IsoDate::Parse(fields[12]);
	decoded.updated_at = IsoDate::Parse(fields[13]);
	decoded.anilist_id = IntOrZero(fields[14]);
	return decoded;
}

std::string EncodeEntry(const Entry& e) {
	return std::format(
	    "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}", e.id, e.mal_id, TypeKey(e.type),
	    EscapeField(e.title), EscapeField(StatusToString(e.status, e.type)), e.score, e.progress,
	    e.total_units, e.volumes, e.total_volumes, e.rewatched, FormatDate(e.start_date),
	    FormatDate(e.finish_date), FormatDate(e.updated_at), e.anilist_id);
}

}  // namespace catalist::csv_codec
