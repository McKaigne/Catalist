#ifndef CATALIST_TUI_THEME_STATUS_STYLE_HPP_
#define CATALIST_TUI_THEME_STATUS_STYLE_HPP_

#include <ftxui/screen/color.hpp>
#include <string>

#include "catalist/model/entry.hpp"

namespace catalist {

[[nodiscard]] std::string statusToIcon(watchStatus status);
[[nodiscard]] ftxui::Color statusToColor(watchStatus status);
[[nodiscard]] std::string typeToIcon(mediaType type);
[[nodiscard]] ftxui::Color typeToColor(mediaType type);
[[nodiscard]] std::string renderStarMeter(int score);

}  // namespace catalist

#endif  // CATALIST_TUI_THEME_STATUS_STYLE_HPP_
