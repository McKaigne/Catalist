#ifndef CATALIST_MENU_OUTPUT_HPP_
#define CATALIST_MENU_OUTPUT_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/date.hpp"

namespace catalist::output {

[[nodiscard]] std::string DateText(const std::optional<IsoDate>& value);

// A score out of 10 as five Nerd Font stars (nf-md-star, nf-md-star_half_full
// and nf-md-star_outline: full, a whole star half filled, and empty), each
// half a point, rounded to the nearest: 7 is three full stars, a half filled
// one and an empty one. "-" for no score.
[[nodiscard]] std::string Stars(double out_of_ten);

[[nodiscard]] std::string Paint(const std::string& value, const char* sgr, bool color);

inline const char* const kBold = "1";
inline const char* const kDim = "2";
inline const char* const kGreen = "32";
inline const char* const kYellow = "33";
inline const char* const kCyan = "36";

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
