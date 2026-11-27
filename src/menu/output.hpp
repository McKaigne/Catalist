#ifndef CATALIST_MENU_OUTPUT_HPP_
#define CATALIST_MENU_OUTPUT_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/date.hpp"

namespace catalist::output {

[[nodiscard]] std::string DateText(const std::optional<IsoDate>& value);

// A score out of 10 as five stars, each worth two points: * full, + half and
// - empty, rounded to the nearest half: 7 is "***+-". "-" for no score.
[[nodiscard]] std::string Stars(double out_of_ten);

[[nodiscard]] std::string Paint(const std::string& value, const char* sgr, bool color);

inline const char* const kBold = "1";
inline const char* const kDim = "2";
inline const char* const kRed = "31";
inline const char* const kGreen = "32";
inline const char* const kYellow = "33";
inline const char* const kMagenta = "35";
inline const char* const kCyan = "36";
inline const char* const kGray = "90";

// A bar of # for what is done and - for what is left, in brackets.
[[nodiscard]] std::string Bar(int done, int total, int width, bool color);
[[nodiscard]] std::string Rule(int width, bool color);

struct TableColumn {
	bool align_right = false;
	bool flexible = false;
};

struct Styled {
	std::string text;
	const char* sgr = nullptr;
};

[[nodiscard]] std::string Rows(const std::vector<TableColumn>& columns,
                               const std::vector<std::vector<Styled>>& rows, int width, bool color);

struct Section {
	std::string heading;
	std::vector<std::vector<Styled>> rows;
};

[[nodiscard]] std::string Sections(const std::vector<TableColumn>& columns,
                                   const std::vector<Section>& sections, int width, bool color);

}  // namespace catalist::output

#endif  // CATALIST_MENU_OUTPUT_HPP_
