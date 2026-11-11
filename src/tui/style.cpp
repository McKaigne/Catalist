#include "tui/style.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>

namespace catalist {

std::string statusToIcon(watchStatus status) {
  switch (status) {
    case watchStatus::kWatching:
      return icon::kWatching;
    case watchStatus::kCompleted:
      return icon::kCompleted;
    case watchStatus::kOnHold:
      return icon::kOnHold;
    case watchStatus::kDropped:
      return icon::kDropped;
    case watchStatus::kPlanToWatch:
      return icon::kPlan;
  }
  return icon::kPlan;
}

ftxui::Color statusToColor(watchStatus status) {
  switch (status) {
    case watchStatus::kWatching:
      return theme::kGreen;
    case watchStatus::kCompleted:
      return theme::kMauve;
    case watchStatus::kOnHold:
      return theme::kYellow;
    case watchStatus::kDropped:
      return theme::kRed;
    case watchStatus::kPlanToWatch:
      return theme::kOverlay0;
  }
  return theme::kSubtext;
}

std::string typeToIcon(mediaType type) {
  switch (type) {
    case mediaType::kAnime:
      return icon::kAnime;
    case mediaType::kManga:
      return icon::kManga;
  }
  return icon::kAnime;
}

ftxui::Color typeToColor(mediaType type) {
  switch (type) {
    case mediaType::kAnime:
      return theme::kBlue;
    case mediaType::kManga:
      return theme::kPeach;
  }
  return theme::kBlue;
}

std::string renderStarMeter(int score) {
  score = std::clamp(score, 0, 10);
  std::string meter;
  // Each of the five stars stands for two points.
  for (int star = 0; star < 5; ++star) {
    int points = score - star * 2;
    meter += star > 0 ? " " : "";
    meter += points >= 2   ? icon::kStarFull
             : points == 1 ? icon::kStarHalf
                           : icon::kStarEmpty;
  }
  return meter;
}

}  // namespace catalist

namespace catalist {

using namespace ftxui;

Element pill(const std::string& label, Color fg, Color bg, bool boldText) {
  static const std::string kCapLeft = "\xEE\x82\xB6";
  static const std::string kCapRight = "\xEE\x82\xB4";
  auto content = text(label) | color(fg) | bgcolor(bg);
  if (boldText)
    content = content | bold;
  return hbox(
      {text(kCapLeft) | color(bg), content, text(kCapRight) | color(bg)});
}

}  // namespace catalist

namespace catalist::card {

using namespace ftxui;

Element frame(const std::string& heading, Element body, bool focused,
              const std::string& badge) {
  return vbox({hbox({text("  " + heading) | bold | color(theme::kLavender),
                     filler(), text(badge) | color(theme::kPeach), text(" ")}),
               text(""), std::move(body) | flex}) |
         border | color(focused ? theme::kMauve : theme::kSurface0);
}

Element section(const std::string& label) {
  return text("  " + label) | bold | color(theme::kOverlay0);
}

Element field(bool active, const std::string& label, Element value,
              int labelWidth) {
  return hbox({text(active ? "▌ " : "  ") | color(theme::kMauve) | bold,
               text(label) | size(WIDTH, EQUAL, labelWidth) |
                   (active ? bold : nothing) |
                   color(active ? theme::kPeach : theme::kSubtext),
               std::move(value) | flex});
}

Element choice(bool active, const std::string& label, Color tint) {
  return hbox({text(active ? "▌ " : "  ") | color(theme::kMauve) | bold,
               text(label) | (active ? bold : nothing) |
                   color(active ? theme::kPeach : tint)});
}

}  // namespace catalist::card

namespace catalist::marquee {

namespace {

size_t utf8SequenceLength(unsigned char lead) {
  if ((lead & 0x80) == 0)
    return 1;
  if ((lead & 0xE0) == 0xC0)
    return 2;
  if ((lead & 0xF0) == 0xE0)
    return 3;
  if ((lead & 0xF8) == 0xF0)
    return 4;
  return 1;
}

uint64_t steadyNowMs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

}  // namespace

std::vector<std::string> splitGlyphs(const std::string& text) {
  std::vector<std::string> glyphs;
  size_t position = 0;
  while (position < text.size()) {
    size_t length =
        utf8SequenceLength(static_cast<unsigned char>(text[position]));
    length = std::min(length, text.size() - position);
    glyphs.push_back(text.substr(position, length));
    position += length;
  }
  return glyphs;
}

std::string clip(const std::string& text, size_t maxGlyphs) {
  auto glyphs = splitGlyphs(text);
  if (glyphs.size() <= maxGlyphs)
    return text;
  std::string clipped;
  for (size_t i = 0; i < maxGlyphs; ++i)
    clipped += glyphs[i];
  return clipped;
}

std::string scroll(const std::string& text, size_t maxGlyphs,
                   uint64_t elapsedMs, uint64_t pauseMs, uint64_t stepMs) {
  if (splitGlyphs(text).size() <= maxGlyphs)
    return text;

  auto loop = splitGlyphs(text + "   •   ");
  uint64_t cycleMs = pauseMs + loop.size() * stepMs;
  uint64_t cycleTime = elapsedMs % cycleMs;
  size_t offset =
      cycleTime < pauseMs
          ? 0
          : static_cast<size_t>((cycleTime - pauseMs) / stepMs) % loop.size();

  std::string window;
  for (size_t i = 0; i < maxGlyphs; ++i)
    window += loop[(offset + i) % loop.size()];
  return window;
}

uint64_t selectionTimer::elapsedFor(int selectedIndex) {
  uint64_t now = steadyNowMs();
  if (trackedIndex_ != selectedIndex) {
    trackedIndex_ = selectedIndex;
    changedAtMs_ = now;
  }
  return now >= changedAtMs_ ? now - changedAtMs_ : 0;
}

}  // namespace catalist::marquee

namespace catalist::keys {

using ftxui::Event;

bool isMoveUp(const Event& event) {
  return event == Event::ArrowUp || isCharacter(event, 'k');
}

bool isMoveDown(const Event& event) {
  return event == Event::ArrowDown || isCharacter(event, 'j');
}

bool isMoveLeft(const Event& event) {
  return event == Event::ArrowLeft || isCharacter(event, 'h');
}

bool isMoveRight(const Event& event) {
  return event == Event::ArrowRight || isCharacter(event, 'l');
}

bool isJumpTop(const Event& event) {
  return event == Event::Home || isCharacter(event, 'g');
}

bool isJumpBottom(const Event& event) {
  return event == Event::End || isCharacter(event, 'G');
}

bool isConfirm(const Event& event) {
  return event == Event::Return;
}

bool isConfirmOrSpace(const Event& event) {
  return event == Event::Return || event == Event::Character(' ');
}

bool isIncrement(const Event& event) {
  return isCharacter(event, '+') || isCharacter(event, '=');
}

bool isCharacter(const Event& event, char expected) {
  return event == Event::Character(std::string(1, expected));
}

}  // namespace catalist::keys

namespace catalist {

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

bool textField::handle(const ftxui::Event& event) {
  if (event == ftxui::Event::Backspace) {
    auto glyphs = marquee::splitGlyphs(value);
    if (!glyphs.empty())
      value.resize(value.size() - glyphs.back().size());
    return true;
  }
  if (!event.is_character())
    return false;
  value += event.character();
  return true;
}

ftxui::Element textField::render(const std::string& placeholder,
                                 bool active) const {
  using namespace ftxui;
  Element shown = value.empty() ? text(placeholder) | color(theme::kOverlay0)
                                : text(value) | color(active ? theme::kPeach
                                                             : theme::kText);
  if (!active)
    return shown;
  Element cursor = text(" ") | inverted;
  return value.empty() ? hbox({cursor, shown}) : hbox({shown, cursor});
}

}  // namespace catalist
