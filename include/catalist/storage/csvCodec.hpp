#ifndef CATALIST_STORAGE_CSV_CODEC_HPP_
#define CATALIST_STORAGE_CSV_CODEC_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalist/core/date.hpp"
#include "catalist/core/result.hpp"
#include "catalist/model/entry.hpp"

// RFC 4180 encoding and decoding of the catalogue files. Pure text in,
// records out; reading and writing the files is the database's job.
namespace catalist::csvCodec {

[[nodiscard]] std::string catalogFileName(mediaType type);

[[nodiscard]] std::string escapeField(const std::string& field);

// Splits text into records, joining quoted fields that span lines. Blank
// lines and '#' comment lines between records are skipped; CRLF is accepted.
[[nodiscard]] std::vector<std::string> splitRecords(std::string_view text);
[[nodiscard]] std::vector<std::string> splitFields(std::string_view record);

[[nodiscard]] std::optional<isoDate> decodeDate(const std::string& field);

// The 16 columns of encodeEntries. Columns after them (written by 0.3 and
// earlier builds) are ignored; a row without the type column is in an older
// layout and is not read.
[[nodiscard]] std::optional<entry> decodeEntry(
    const std::vector<std::string>& fields, mediaType type);
// Fails on the first row decodeEntry cannot read, so a file is never loaded
// half-read.
[[nodiscard]] result<std::vector<entry>> decodeEntries(std::string_view text,
                                                       mediaType type);
[[nodiscard]] std::string encodeEntries(const std::vector<entry>& entries);

}  // namespace catalist::csvCodec

#endif  // CATALIST_STORAGE_CSV_CODEC_HPP_
