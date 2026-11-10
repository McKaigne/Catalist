#ifndef CATALIST_STORAGE_CSV_CODEC_HPP_
#define CATALIST_STORAGE_CSV_CODEC_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalist/core/date.hpp"
#include "catalist/model/activity.hpp"
#include "catalist/model/syncBase.hpp"
#include "catalist/model/entry.hpp"

// RFC 4180 encoding and decoding of the catalogue and history files. Pure
// text in, records out; reading and writing the files is the database's job.
namespace catalist::csvCodec {

[[nodiscard]] std::string catalogFileName(mediaType type);
[[nodiscard]] std::string historyFileName();
[[nodiscard]] std::string syncFileName();

[[nodiscard]] std::string escapeField(const std::string& field);

// Splits text into records, joining quoted fields that span lines. Blank
// lines and '#' comment lines between records are skipped; CRLF is accepted.
[[nodiscard]] std::vector<std::string> splitRecords(std::string_view text);
[[nodiscard]] std::vector<std::string> splitFields(std::string_view record);

[[nodiscard]] std::optional<isoDate> decodeDate(const std::string& field);

// Accepts every historical layout: the 7-column legacy file, the 16-column
// file without a type column, and the current file with type and
// external_ids columns.
[[nodiscard]] std::optional<entry> decodeEntry(
    const std::vector<std::string>& fields, mediaType type);
[[nodiscard]] std::vector<entry> decodeEntries(std::string_view text,
                                               mediaType type);
[[nodiscard]] std::string encodeEntries(const std::vector<entry>& entries);

[[nodiscard]] std::vector<syncBase> decodeSyncBases(std::string_view text);
[[nodiscard]] std::string encodeSyncBases(const std::vector<syncBase>& bases);

[[nodiscard]] std::vector<activityRecord> decodeHistory(std::string_view text);
[[nodiscard]] std::string encodeHistory(
    const std::vector<activityRecord>& records);

}  // namespace catalist::csvCodec

#endif  // CATALIST_STORAGE_CSV_CODEC_HPP_
