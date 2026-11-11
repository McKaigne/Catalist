#ifndef CATALIST_TUI_STYLE_HPP_
#define CATALIST_TUI_STYLE_HPP_

#include <cstdint>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <optional>
#include <string>
#include <vector>

#include "entry.hpp"

namespace catalist {

namespace theme {
const auto kCrust = ftxui::Color::RGB(0x11, 0x11, 0x1b);
const auto kText = ftxui::Color::RGB(0xcd, 0xd6, 0xf4);
const auto kSubtext = ftxui::Color::RGB(0xa6, 0xad, 0xc8);
const auto kOverlay0 = ftxui::Color::RGB(0x6c, 0x70, 0x86);
const auto kSurface0 = ftxui::Color::RGB(0x31, 0x32, 0x44);
const auto kSurface1 = ftxui::Color::RGB(0x45, 0x47, 0x5a);
const auto kMauve = ftxui::Color::RGB(0xcb, 0xa6, 0xf7);
const auto kLavender = ftxui::Color::RGB(0xb4, 0xbe, 0xfe);
const auto kBlue = ftxui::Color::RGB(0x89, 0xb4, 0xfa);
const auto kGreen = ftxui::Color::RGB(0xa6, 0xe3, 0xa1);
const auto kYellow = ftxui::Color::RGB(0xf9, 0xe2, 0xaf);
const auto kPeach = ftxui::Color::RGB(0xfa, 0xb3, 0x87);
const auto kRed = ftxui::Color::RGB(0xf3, 0x8b, 0xa8);
}  // namespace theme

namespace icon {
const std::string kLibrary = "󰓹";
const std::string kSearch = "";
const std::string kStats = "󰄫";

const std::string kAnime = "󰴙";
const std::string kManga = "󱥬";

const std::string kWatching = "󰐌";
const std::string kCompleted = "󰗠";
const std::string kOnHold = "󰏥";
const std::string kDropped = "󰅙";
const std::string kPlan = "󰥔";

const std::string kCheckSingle = "󰄲";

const std::string kStarFull = "";
const std::string kStarHalf = "";
const std::string kStarEmpty = "";

const std::string kGauge = "󰑋";
}  // namespace icon

}  // namespace catalist

namespace catalist {

[[nodiscard]] std::string statusToIcon(watchStatus status);
[[nodiscard]] ftxui::Color statusToColor(watchStatus status);
[[nodiscard]] std::string typeToIcon(mediaType type);
[[nodiscard]] ftxui::Color typeToColor(mediaType type);
[[nodiscard]] std::string renderStarMeter(int score);

}  // namespace catalist

namespace catalist {

// A label with rounded powerline caps in the background colour.
[[nodiscard]] ftxui::Element pill(const std::string& label, ftxui::Color fg,
                                  ftxui::Color bg, bool boldText = true);

}  // namespace catalist

// The building blocks of the catalogue's look, shared by every tab.
namespace catalist::card {

// A rounded card: bold lavender heading, an optional badge on the right, a
// blank line and the body. The border turns mauve while the card has focus.
[[nodiscard]] ftxui::Element frame(const std::string& heading,
                                   ftxui::Element body, bool focused,
                                   const std::string& badge = "");

// An upper-case sub-heading inside a card ("STATUS", "METRICS").
[[nodiscard]] ftxui::Element section(const std::string& label);

// A labelled row; `active` draws the cursor bar and highlights the label.
[[nodiscard]] ftxui::Element field(bool active, const std::string& label,
                                   ftxui::Element value, int labelWidth = 14);

// A selectable line in a list card, with the same cursor bar.
[[nodiscard]] ftxui::Element choice(bool active, const std::string& label,
                                    ftxui::Color tint);

}  // namespace catalist::card

namespace catalist::marquee {

// Splits UTF-8 text into code points so clipping never cuts a glyph in half.
[[nodiscard]] std::vector<std::string> splitGlyphs(const std::string& text);

[[nodiscard]] std::string clip(const std::string& text, size_t maxGlyphs);

// Shows the first maxGlyphs for pauseMs, then scrolls one glyph per stepMs
// and loops. Text that fits is returned unchanged.
[[nodiscard]] std::string scroll(const std::string& text, size_t maxGlyphs,
                                 uint64_t elapsedMs, uint64_t pauseMs = 2000,
                                 uint64_t stepMs = 250);

// Restarts the scroll whenever the highlighted row changes.
class selectionTimer {
public:
  [[nodiscard]] uint64_t elapsedFor(int selectedIndex);

private:
  std::optional<int> trackedIndex_;
  uint64_t changedAtMs_ = 0;
};

}  // namespace catalist::marquee

// Named predicates for the keys that have more than one spelling: movement
// (arrows always work too), confirmation and the progress step.
namespace catalist {

namespace keys {

[[nodiscard]] bool isMoveUp(const ftxui::Event& event);
[[nodiscard]] bool isMoveDown(const ftxui::Event& event);
[[nodiscard]] bool isMoveLeft(const ftxui::Event& event);
[[nodiscard]] bool isMoveRight(const ftxui::Event& event);
[[nodiscard]] bool isJumpTop(const ftxui::Event& event);
[[nodiscard]] bool isJumpBottom(const ftxui::Event& event);
[[nodiscard]] bool isConfirm(const ftxui::Event& event);
[[nodiscard]] bool isConfirmOrSpace(const ftxui::Event& event);
// '+', or the unshifted '=' that shares its key on most layouts.
[[nodiscard]] bool isIncrement(const ftxui::Event& event);
[[nodiscard]] bool isCharacter(const ftxui::Event& event, char expected);

}  // namespace keys

// ASCII lower case, byte by byte, for matching titles without regard to case.
[[nodiscard]] std::string lowercase(std::string text);

// A one-line text entry, drawn by hand: what was typed, then a block cursor.
struct textField {
  std::string value;

  // Adds typed (or pasted) characters and removes the last glyph on
  // Backspace; false for every other key.
  bool handle(const ftxui::Event& event);
  // The placeholder shows, dimmed, while the field is empty.
  [[nodiscard]] ftxui::Element render(const std::string& placeholder,
                                      bool active) const;
};

}  // namespace catalist

#endif  // CATALIST_TUI_STYLE_HPP_
