#pragma once

#include <ftxui/screen/color.hpp>
#include <string>

namespace catalist {

namespace theme {
  const auto Base     = ftxui::Color::RGB(0x1e, 0x1e, 0x2e);
  const auto Mantle   = ftxui::Color::RGB(0x18, 0x18, 0x25);
  const auto Crust    = ftxui::Color::RGB(0x11, 0x11, 0x1b);
  const auto Text     = ftxui::Color::RGB(0xcd, 0xd6, 0xf4);
  const auto Subtext  = ftxui::Color::RGB(0xa6, 0xad, 0xc8);
  const auto Overlay0 = ftxui::Color::RGB(0x6c, 0x70, 0x86);
  const auto Surface0 = ftxui::Color::RGB(0x31, 0x32, 0x44);
  const auto Surface1 = ftxui::Color::RGB(0x45, 0x47, 0x5a);
  const auto Surface2 = ftxui::Color::RGB(0x58, 0x5b, 0x70);
  const auto Mauve    = ftxui::Color::RGB(0xcb, 0xa6, 0xf7);
  const auto Lavender = ftxui::Color::RGB(0xb4, 0xbe, 0xfe);
  const auto Blue     = ftxui::Color::RGB(0x89, 0xb4, 0xfa);
  const auto Green    = ftxui::Color::RGB(0xa6, 0xe3, 0xa1);
  const auto Yellow   = ftxui::Color::RGB(0xf9, 0xe2, 0xaf);
  const auto Peach    = ftxui::Color::RGB(0xfa, 0xb3, 0x87);
  const auto Red      = ftxui::Color::RGB(0xf3, 0x8b, 0xa8);
}

namespace icon {
  const std::string AppLogo     = "󰄛"; // Cat logo
  const std::string Library     = "󰓹"; // Bookshelf
  const std::string InProgress  = "󰐌"; // Play circle
  const std::string Search      = ""; // Search glass
  const std::string Stats       = "󰄫"; // Dashboard / chart
  const std::string Sync        = "󰓦"; // Sync arrows

  const std::string Anime       = "󰴙"; // Anime icon
  const std::string Manga       = "󱥬"; // Manga icon

  const std::string Watching    = "󰐌"; // Play circle
  const std::string Completed   = "󰗠"; // Checked circle icon
  const std::string OnHold      = "󰏥"; // Pause circle
  const std::string Dropped     = "󰅙"; // Dropped icon
  const std::string Plan        = "󰥔"; // Clock outline

  // Selection Checkbox Glyphs
  const std::string CheckSingle = "󰄲"; // Single-select checked icon
  const std::string CheckMulti  = "󰄸"; // Multi-select checked icon
  const std::string CheckBlank  = "󰄮"; // Unchecked outline

  // 5-Star Glyphs
  const std::string StarFull    = "";
  const std::string StarHalf    = "";
  const std::string StarEmpty   = "";

  const std::string Gauge       = "󰑋";
  const std::string Keyboard    = "󰌌";
  const std::string Web         = "󰖟";
  const std::string Warn        = "⚠️";
}

} // namespace catalist
