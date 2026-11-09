#include "catalist/storage/csvCodec.hpp"

#include <sstream>

namespace catalist::csvCodec {

namespace {

const char* kCatalogHeader =
    "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,"
    "volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at,"
    "dirty,external_ids,queue_pos\n";
const char* kHistoryHeader =
    "#timestamp,type,title,status,progress,total_units,score\n";

constexpr size_t kMinimumCatalogFields = 7;
constexpr size_t kMinimumHistoryFields = 7;
constexpr size_t kOffsetThreeMinimumFields = 16;

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
  return field == "anime" || field == "manga";
}

// Legacy files have no anilist/type columns; 16-column files have anilist
// but no type; current files have both.
size_t titleColumn(const std::vector<std::string>& fields) {
  if (fields.size() >= 4 && isTypeColumn(fields[3]))
    return 4;
  if (fields.size() >= kOffsetThreeMinimumFields)
    return 3;
  return 2;
}

bool togglesQuoteState(const std::string& line, bool insideQuotes) {
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] != '"')
      continue;
    bool escapedQuote = insideQuotes && i + 1 < line.size() && line[i + 1] == '"';
    if (escapedQuote)
      ++i;
    else
      insideQuotes = !insideQuotes;
  }
  return insideQuotes;
}

std::vector<std::string> splitLines(std::string_view text) {
  std::vector<std::string> lines;
  std::istringstream stream{std::string(text)};
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    lines.push_back(line);
  }
  return lines;
}

bool isSkippableLine(const std::string& line) {
  return line.empty() || line[0] == '#';
}

void fillOptionalColumns(entry& decoded, const std::vector<std::string>& fields,
                         size_t offset) {
  auto column = [&](size_t index) -> const std::string* {
    return fields.size() > offset + index ? &fields[offset + index] : nullptr;
  };
  if (auto* value = column(5))
    decoded.volumes = parseInt(*value).value_or(decoded.volumes);
  if (auto* value = column(6))
    decoded.totalVolumes = parseInt(*value).value_or(decoded.totalVolumes);
  if (auto* value = column(7))
    decoded.rewatched = parseInt(*value).value_or(decoded.rewatched);
  if (auto* value = column(8))
    decoded.startDate = decodeDate(*value);
  if (auto* value = column(9))
    decoded.finishDate = decodeDate(*value);
  if (auto* value = column(10))
    decoded.notes = *value;
  if (auto* value = column(11))
    decoded.updatedAt = decodeDate(*value);
  if (auto* value = column(12))
    decoded.dirty = (*value == "1" || *value == "true");
  if (auto* value = column(13); value && !value->empty())
    decoded.remoteIds = deserializeRemoteIds(*value);
  if (auto* value = column(14))
    decoded.queuePos = parseInt(*value);
}

void backfillRemoteIds(entry& decoded) {
  if (decoded.malId > 0 && !decoded.remoteIds.count("mal"))
    decoded.remoteIds["mal"] = std::to_string(decoded.malId);
  if (decoded.anilistId > 0 && !decoded.remoteIds.count("anilist"))
    decoded.remoteIds["anilist"] = std::to_string(decoded.anilistId);
}

}  // namespace

std::string catalogFileName(mediaType type) {
  return type == mediaType::kAnime ? "anime.csv" : "manga.csv";
}

std::string historyFileName() {
  return "history.csv";
}

std::string escapeField(const std::string& field) {
  bool needsQuotes = field.find_first_of(",\"\n\r") != std::string::npos;
  if (!needsQuotes)
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
    if (pending.empty() && isSkippableLine(line))
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
    bool escapedQuote = inQuotes && c == '"' && i + 1 < record.size() &&
                        record[i + 1] == '"';
    if (escapedQuote) {
      field += '"';
      ++i;
    } else if (c == '"') {
      inQuotes = !inQuotes;
    } else if (c == ',' && !inQuotes) {
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
  // unknown date; it and an empty field both mean "no date".
  if (field.empty() || field == "0000-00-00")
    return std::nullopt;
  return isoDate::parse(field);
}

std::optional<entry> decodeEntry(const std::vector<std::string>& fields,
                                 mediaType type) {
  if (fields.size() < kMinimumCatalogFields)
    return std::nullopt;
  auto id = parseInt(fields[0]);
  if (!id)
    return std::nullopt;

  entry decoded;
  decoded.type = type;
  decoded.id = *id;
  decoded.malId = intOrZero(fields[1]);
  if (!fields[2].empty())
    decoded.anilistId = intOrZero(fields[2]);

  size_t offset = titleColumn(fields);
  decoded.title = fields[offset];
  decoded.status = stringToStatus(fields[offset + 1]);
  decoded.score = intOrZero(fields[offset + 2]);
  decoded.progress = intOrZero(fields[offset + 3]);
  decoded.totalUnits = intOrZero(fields[offset + 4]);
  fillOptionalColumns(decoded, fields, offset);
  backfillRemoteIds(decoded);
  return decoded;
}

std::vector<entry> decodeEntries(std::string_view text, mediaType type) {
  std::vector<entry> entries;
  for (const auto& record : splitRecords(text)) {
    if (auto decoded = decodeEntry(splitFields(record), type))
      entries.push_back(std::move(*decoded));
  }
  return entries;
}

std::string encodeEntries(const std::vector<entry>& entries) {
  std::ostringstream out;
  out << kCatalogHeader;
  for (const auto& e : entries) {
    out << e.id << "," << e.malId << "," << e.anilistId << ","
        << typeKey(e.type) << "," << escapeField(e.title) << ","
        << escapeField(statusToString(e.status, e.type)) << "," << e.score
        << "," << e.progress << "," << e.totalUnits << "," << e.volumes << ","
        << e.totalVolumes << "," << e.rewatched << ","
        << formatDate(e.startDate) << "," << formatDate(e.finishDate) << ","
        << escapeField(e.notes) << "," << formatDate(e.updatedAt) << ","
        << (e.dirty ? "1" : "0") << ","
        << escapeField(serializeRemoteIds(e.remoteIds)) << ","
        << (e.queuePos ? std::to_string(*e.queuePos) : "") << "\n";
  }
  return out.str();
}

std::vector<activityRecord> decodeHistory(std::string_view text) {
  std::vector<activityRecord> records;
  for (const auto& line : splitLines(text)) {
    if (isSkippableLine(line))
      continue;
    auto fields = splitFields(line);
    if (fields.size() < kMinimumHistoryFields)
      continue;
    activityRecord record;
    record.timestamp = fields[0];
    record.type = fields[1] == "anime" ? mediaType::kAnime : mediaType::kManga;
    record.title = fields[2];
    record.status = stringToStatus(fields[3]);
    record.progress = intOrZero(fields[4]);
    record.totalUnits = intOrZero(fields[5]);
    record.score = intOrZero(fields[6]);
    records.push_back(std::move(record));
  }
  return records;
}

std::string encodeHistory(const std::vector<activityRecord>& records) {
  std::ostringstream out;
  out << kHistoryHeader;
  for (const auto& h : records) {
    out << escapeField(h.timestamp) << "," << typeKey(h.type) << ","
        << escapeField(h.title) << ","
        << escapeField(statusToString(h.status, h.type)) << "," << h.progress
        << "," << h.totalUnits << "," << h.score << "\n";
  }
  return out.str();
}

}  // namespace catalist::csvCodec
