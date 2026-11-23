#ifndef CATALIST_STORAGE_CSV_CODEC_HPP_
#define CATALIST_STORAGE_CSV_CODEC_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/entry.hpp"

// RFC 4180 encoding and decoding of catalogue rows: the form a title takes
// in the undo history. Pure text in, records out.
namespace catalist::csv_codec {

[[nodiscard]] std::string EscapeField(const std::string& field);

[[nodiscard]] std::vector<std::string> SplitFields(std::string_view record);

// The fields of EncodeEntry; nothing for a row with fewer, or without the
// type in its place.
[[nodiscard]] std::optional<Entry> DecodeEntry(const std::vector<std::string>& fields);
// id, mal_id, type, title, status, score, progress, total_units, volumes,
// total_volumes, rewatched, start_date, finish_date, updated_at, released,
// checked_at and anilist_id; no line end.
[[nodiscard]] std::string EncodeEntry(const Entry& row);

}  // namespace catalist::csv_codec

#endif  // CATALIST_STORAGE_CSV_CODEC_HPP_
