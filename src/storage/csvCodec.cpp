#include "catalist/storage/csvCodec.hpp"

#include <sstream>

namespace catalist::csvCodec {

namespace {

const char* kCatalogHeader =
    "#id,mal_id,anilist_id,type,title,status,score,progress,total_units,"
    "volumes,total_volumes,rewatched,start_date,finish_date,notes,updated_at\n";

constexpr size_t kCatalogFields = 16;

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
  // unknown date; parse rejects it (there is no year 0), so it and an empty
  // field both mean "no date".
  return isoDate::parse(field);
}

std::optional<entry> decodeEntry(const std::vector<std::string>& fields,
                                 mediaType type) {
  bool currentLayout = fields.size() >= kCatalogFields && isTypeColumn(fields[3]);
  auto id = currentLayout ? parseInt(fields[0]) : std::nullopt;
  if (!id)
    return std::nullopt;

  entry decoded;
  decoded.type = type;
  decoded.id = *id;
  decoded.malId = intOrZero(fields[1]);
  decoded.anilistId = intOrZero(fields[2]);
  decoded.title = fields[4];
  decoded.status = decodeStatus(fields[5]);
  decoded.score = intOrZero(fields[6]);
  decoded.progress = intOrZero(fields[7]);
  decoded.totalUnits = intOrZero(fields[8]);
  decoded.volumes = intOrZero(fields[9]);
  decoded.totalVolumes = intOrZero(fields[10]);
  decoded.rewatched = intOrZero(fields[11]);
  decoded.startDate = decodeDate(fields[12]);
  decoded.finishDate = decodeDate(fields[13]);
  decoded.notes = fields[14];
  decoded.updatedAt = decodeDate(fields[15]);
  return decoded;
}

result<std::vector<entry>> decodeEntries(std::string_view text,
                                         mediaType type) {
  std::vector<entry> entries;
  for (const auto& record : splitRecords(text)) {
    auto decoded = decodeEntry(splitFields(record), type);
    if (!decoded)
      return result<std::vector<entry>>::failure(
          "row " + std::to_string(entries.size() + 1) +
          " has a column layout this version cannot read");
    entries.push_back(std::move(*decoded));
  }
  return result<std::vector<entry>>::success(std::move(entries));
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
        << escapeField(e.notes) << "," << formatDate(e.updatedAt) << "\n";
  }
  return out.str();
}

}  // namespace catalist::csvCodec
