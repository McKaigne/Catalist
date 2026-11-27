#include "storage/csv_codec.hpp"

#include <charconv>
#include <format>
#include <utility>

namespace catalist::csv_codec {

namespace {

constexpr size_t kFields = 18;
constexpr size_t kOldFields = 17;

std::optional<int> ParseCount(const std::string& field) {
	int value = 0;
	const char* last = field.data() + field.size();
	auto [end, error] = std::from_chars(field.data(), last, value);
	if (field.empty() or error != std::errc() or end != last or value < 0)
		return std::nullopt;
	return value;
}

bool ReadDate(const std::string& field, std::optional<IsoDate>& date) {
	date = IsoDate::Parse(field);
	return field.empty() or date.has_value();
}

}  // namespace

std::string EscapeField(const std::string& field) {
	if (field.find_first_of(",\"\n\r") == std::string::npos)
		return field;
	std::string escaped = "\"";
	for (char c : field) {
		if (c == '"')
			escaped += "\"\"";
		else
			escaped += c;
	}
	return escaped + "\"";
}

std::vector<std::string> SplitRecords(std::string_view text) {
	std::vector<std::string> records;
	std::string record;
	bool in_quotes = false;
	auto finish = [&] {
		if (not record.empty() and record.back() == '\r')
			record.pop_back();
		if (not record.empty())
			records.push_back(record);
		record.clear();
	};
	for (char c : text) {
		if (c == '"')
			in_quotes = not in_quotes;
		if (c == '\n' and not in_quotes)
			finish();
		else
			record += c;
	}
	finish();
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

std::optional<Entry> DecodeEntry(const std::vector<std::string>& fields) {
	if ((fields.size() != kFields and fields.size() != kOldFields) or (fields[1] != "anime" and fields[1] != "manga") or
	    fields[2].empty())
		return std::nullopt;
	auto status = ParseStatus(fields[4]);
	auto format =
	    fields.size() == kFields ? ParseMediaFormat(fields[17]) : std::optional(MediaFormat::kUnknown);
	if (not status or not format)
		return std::nullopt;

	Entry row;
	row.type = fields[1] == "manga" ? MediaType::kManga : MediaType::kAnime;
	row.title = fields[2];
	row.english_title = fields[3];
	row.status = *status;
	row.format = *format;
	row.changed_at = fields[14];
	const std::pair<size_t, int*> counts[] = {
	    {0, &row.id},      {5, &row.score},         {6, &row.progress},   {7, &row.total_units},
	    {8, &row.volumes}, {9, &row.total_volumes}, {10, &row.rewatched}, {15, &row.mal_id},
	    {16, &row.anilist_id}};
	for (const auto& [column, value] : counts) {
		auto number = ParseCount(fields[column]);
		if (not number)
			return std::nullopt;
		*value = *number;
	}
	bool dates = ReadDate(fields[11], row.start_date) and ReadDate(fields[12], row.finish_date) and
	             ReadDate(fields[13], row.updated_at);
	if (not dates or row.id == 0 or row.score > 10)
		return std::nullopt;
	return row;
}

std::string EncodeEntry(const Entry& e) {
	return std::format("{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}", e.id, TypeKey(e.type),
	                   EscapeField(e.title), EscapeField(e.english_title), StatusKey(e.status),
	                   e.score, e.progress, e.total_units, e.volumes, e.total_volumes, e.rewatched,
	                   FormatDate(e.start_date), FormatDate(e.finish_date),
	                   FormatDate(e.updated_at), e.changed_at, e.mal_id, e.anilist_id,
	                   MediaFormatKey(e.format));
}

}  // namespace catalist::csv_codec
