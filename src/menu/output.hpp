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
inline const char* const kRed = "31";
inline const char* const kGreen = "32";
inline const char* const kYellow = "33";
inline const char* const kMagenta = "35";
inline const char* const kCyan = "36";
inline const char* const kGray = "90";

// Nerd Font icons, the filled circle form wherever one exists.
namespace icon {
inline const char* const kAdd = "\U000F0417";
inline const char* const kAddBoth = "\U000F034C";
inline const char* const kList = "\U000F0572";
inline const char* const kLog = "\U000F040C";
inline const char* const kEdit = "\U000F06FF";
inline const char* const kRemove = "\U000F0683";
inline const char* const kStats = "\U000F154D";
inline const char* const kTransfer = "\U000F0FE1";
inline const char* const kExit = "\U000F0206";
inline const char* const kBack = "\U000F0CDD";
inline const char* const kHome = "\U000F07D2";
inline const char* const kAnime = "\U000F0502";
inline const char* const kManga = "\U000F14F7";
inline const char* const kFilter = "\U000F0232";
inline const char* const kSearch = "\U000F0349";
inline const char* const kInfo = "\U000F02FC";
inline const char* const kPlus = "\U000F0417";
inline const char* const kMinus = "\U000F0376";
inline const char* const kProgress = "\U000F0995";
inline const char* const kScore = "\U000F04CF";
inline const char* const kStatus = "\U000F023B";
inline const char* const kTotal = "\U000F0424";
inline const char* const kStart = "\U000F00ED";
inline const char* const kFinish = "\U000F00EF";
inline const char* const kRename = "\U000F060E";
inline const char* const kRewatch = "\U000F1378";
inline const char* const kImport = "\U000F1464";
inline const char* const kExport = "\U000F0CE1";
inline const char* const kNext = "\U000F0B2A";
inline const char* const kPrevious = "\U000F0B28";
inline const char* const kError = "\U000F0028";
inline const char* const kDone = "\U000F05E0";
inline const char* const kPrompt = "\U000F0056";
inline const char* const kArrow = "\U000F0CDF";
inline const char* const kWatching = "\U000F040C";
inline const char* const kCompleted = "\U000F05E0";
inline const char* const kOnHold = "\U000F03E5";
inline const char* const kDropped = "\U000F0159";
inline const char* const kPlanned = "\U000F0954";
inline const char* const kRewatching = "\U000F1378";
}  // namespace icon

// A number as filled numeric circles, one per digit, padded on the left to
// `digits` columns.
[[nodiscard]] std::string NumberHint(int number, int digits = 1);
// A letter key as a filled alpha circle.
[[nodiscard]] std::string LetterHint(char letter);

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
