#ifndef CATALIST_STORAGE_CSV_CODEC_HPP_
#define CATALIST_STORAGE_CSV_CODEC_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/entry.hpp"

namespace catalist::csv_codec {

inline constexpr const char* kHeader =
    "id,type,title,english_title,status,score,progress,total_units,volumes,total_volumes,"
    "rewatched,start_date,finish_date,updated_at,changed_at,mal_id,anilist_id,rewatch_value";
// The header before rewatch_value was added; such a file still loads.
inline constexpr const char* kOldHeader =
    "id,type,title,english_title,status,score,progress,total_units,volumes,total_volumes,"
    "rewatched,start_date,finish_date,updated_at,changed_at,mal_id,anilist_id";

[[nodiscard]] std::string EscapeField(const std::string& field);

// One record per line, except that a line break inside quotes stays in its
// record.
[[nodiscard]] std::vector<std::string> SplitRecords(std::string_view text);
[[nodiscard]] std::vector<std::string> SplitFields(std::string_view record);

// Nothing for a row that is not exactly what EncodeEntry writes, so a damaged
// file is reported instead of being saved over.
[[nodiscard]] std::optional<Entry> DecodeEntry(const std::vector<std::string>& fields);
[[nodiscard]] std::string EncodeEntry(const Entry& row);

}  // namespace catalist::csv_codec

#endif  // CATALIST_STORAGE_CSV_CODEC_HPP_
