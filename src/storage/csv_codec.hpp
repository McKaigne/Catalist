#ifndef CATALIST_STORAGE_CSV_CODEC_HPP_
#define CATALIST_STORAGE_CSV_CODEC_HPP_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/entry.hpp"

namespace catalist::csv_codec {

[[nodiscard]] std::string EscapeField(const std::string& field);

[[nodiscard]] std::vector<std::string> SplitFields(std::string_view record);

[[nodiscard]] std::optional<Entry> DecodeEntry(const std::vector<std::string>& fields);
[[nodiscard]] std::string EncodeEntry(const Entry& row);

}  // namespace catalist::csv_codec

#endif  // CATALIST_STORAGE_CSV_CODEC_HPP_
