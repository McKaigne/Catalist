#ifndef CATALIST_STORAGE_CSV_CODEC_HPP_
#define CATALIST_STORAGE_CSV_CODEC_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/date.hpp"
#include "core/entry.hpp"
#include "core/result.hpp"

// RFC 4180 encoding and decoding of catalogue rows: the form a title takes
// in the undo history (Catalist-TUI's too), and the files Catalist 0.3.0
// and earlier kept the lists in, which `import` reads. Pure text in,
// records out.
namespace catalist::csvCodec {

[[nodiscard]] std::string escapeField(const std::string& field);

// Splits text into records, joining quoted fields that span lines. Blank
// lines and '#' comment lines between records are skipped; CRLF is accepted.
[[nodiscard]] std::vector<std::string> splitRecords(std::string_view text);
[[nodiscard]] std::vector<std::string> splitFields(std::string_view record);

[[nodiscard]] std::optional<isoDate> decodeDate(const std::string& field);

// The 17 or 18 fields of encodeEntry (or their first 14 or 16, written
// before the web checks and AniList ids), or the 16 that Catalist 0.2.1 to
// 0.3.0 wrote (their notes are dropped). Columns after them are ignored; a
// row without the type column where either layout has it is in an older
// layout and is not read. The type is the row's own.
[[nodiscard]] std::optional<entry> decodeEntry(const std::vector<std::string>& fields);
// A whole file of rows, as Catalist 0.3.0 and earlier wrote anime.csv and
// manga.csv. Fails on the first row decodeEntry cannot read, so a file is
// never read half.
[[nodiscard]] result<std::vector<entry>> decodeEntries(std::string_view text);
// id, mal_id, type, title, status, score, progress, total_units, volumes,
// total_volumes, rewatched, start_date, finish_date, updated_at, released,
// checked_at, anilist_id and, when there is one, the Kitsu id; no line end.
[[nodiscard]] std::string encodeEntry(const entry& row);

}  // namespace catalist::csvCodec

#endif  // CATALIST_STORAGE_CSV_CODEC_HPP_
