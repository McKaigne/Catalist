#ifndef CATALIST_OUTPUT_HPP_
#define CATALIST_OUTPUT_HPP_

#include <optional>
#include <string>
#include <vector>

#include "date.hpp"

// The three forms the command line prints rows in: an aligned table for a
// terminal, tab-separated lines for a pipe, and JSON Lines. Pure text out.
namespace catalist::output {

// One field of a row. `text` is how it reads in a table or a tab-separated
// line; `json` says how JSON writes it.
enum class jsonForm { kString, kNumber, kNull };
struct cell {
	std::string text;
	jsonForm json = jsonForm::kString;
};

[[nodiscard]] cell text(std::string value);
[[nodiscard]] cell number(long long value);
// A count that is 0 while it is not known: 0 in a line, null in JSON.
[[nodiscard]] cell known(int value);
// A date that may be absent: "-" in a line, null in JSON.
[[nodiscard]] cell date(const std::optional<isoDate>& value);
// Text that may be empty: "-" in a line, null in JSON.
[[nodiscard]] cell maybe(const std::string& value);

// One line of tab-separated fields, with its line end. A tab or a line break
// inside a field becomes one space, so every line has the same fields.
[[nodiscard]] std::string tsvLine(const std::vector<std::string>& fields);
[[nodiscard]] std::string tsvLine(const std::vector<cell>& cells);

// The text as a JSON string, quotes included.
[[nodiscard]] std::string jsonString(const std::string& value);
// One JSON object on one line, with its line end: `names[i]` is the key of
// `cells[i]`.
[[nodiscard]] std::string jsonLine(const std::vector<std::string>& names,
                                   const std::vector<cell>& cells);

// The text in an SGR colour or style ("1", "32"...), or as it is when
// `color` is off or there is no style.
[[nodiscard]] std::string paint(const std::string& value, const char* sgr, bool color);

inline const char* const kBold = "1";
inline const char* const kDim = "2";
inline const char* const kGreen = "32";
inline const char* const kCyan = "36";

struct tableColumn {
	std::string header;
	bool alignRight = false;
	// The one column that gives way when the table is wider than the
	// terminal (the title).
	bool flexible = false;
};

// A table cell, with the style it is painted in (none when null).
struct styled {
	std::string text;
	const char* sgr = nullptr;
};

// The header and the rows, every column as wide as what it holds (a CJK
// glyph counts two), two spaces apart, at most `width` columns across.
[[nodiscard]] std::string table(const std::vector<tableColumn>& columns,
                                const std::vector<std::vector<styled>>& rows, int width,
                                bool color);

}  // namespace catalist::output

#endif  // CATALIST_OUTPUT_HPP_
