#ifndef CATALIST_CLI_OUTPUT_HPP_
#define CATALIST_CLI_OUTPUT_HPP_

#include <optional>
#include <string>
#include <vector>

#include "core/date.hpp"

// The two forms the command line prints rows in: an aligned table for a
// terminal, and tab-separated lines for a pipe. Pure text out.
namespace catalist::output {

// A date that may be absent, or text that may be empty: "-" then.
[[nodiscard]] std::string DateText(const std::optional<IsoDate>& value);
[[nodiscard]] std::string OrDash(const std::string& value);

// One line of tab-separated fields, with its line end. A tab or a line break
// inside a field becomes one space, so every line has the same fields.
[[nodiscard]] std::string TsvLine(const std::vector<std::string>& fields);

// A score out of 10 as five Nerd Font stars (nf-md-star, nf-md-star_half_full
// and nf-md-star_outline: full, a whole star half filled, and empty), each
// half a point, rounded to the nearest: 7 is three full stars, a half filled
// one and an empty one. "-" for no score.
[[nodiscard]] std::string Stars(double out_of_ten);

// The text in an SGR colour or style ("1", "32"...), or as it is when
// `color` is off or there is no style.
[[nodiscard]] std::string Paint(const std::string& value, const char* sgr, bool color);

inline const char* const kBold = "1";
inline const char* const kDim = "2";
inline const char* const kRed = "31";
inline const char* const kGreen = "32";
inline const char* const kYellow = "33";
inline const char* const kCyan = "36";

struct TableColumn {
	std::string header;
	bool align_right = false;
	// The one column that gives way when the table is wider than the
	// terminal (the title).
	bool flexible = false;
};

// A table cell, with the style it is painted in (none when null).
struct Styled {
	std::string text;
	const char* sgr = nullptr;
};

// The header and the rows, every column as wide as what it holds (a CJK
// glyph counts two), two spaces apart, at most `width` columns across.
[[nodiscard]] std::string Table(const std::vector<TableColumn>& columns,
                                const std::vector<std::vector<Styled>>& rows, int width,
                                bool color);

}  // namespace catalist::output

#endif  // CATALIST_CLI_OUTPUT_HPP_
