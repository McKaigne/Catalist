#pragma once

#include <ftxui/screen/color.hpp>
#include <string>

namespace catalist {

namespace theme {
const auto kBase = ftxui::Color::RGB(0x1e, 0x1e, 0x2e);
const auto kMantle = ftxui::Color::RGB(0x18, 0x18, 0x25);
const auto kCrust = ftxui::Color::RGB(0x11, 0x11, 0x1b);
const auto kText = ftxui::Color::RGB(0xcd, 0xd6, 0xf4);
const auto kSubtext = ftxui::Color::RGB(0xa6, 0xad, 0xc8);
const auto kOverlay0 = ftxui::Color::RGB(0x6c, 0x70, 0x86);
const auto kSurface0 = ftxui::Color::RGB(0x31, 0x32, 0x44);
const auto kSurface1 = ftxui::Color::RGB(0x45, 0x47, 0x5a);
const auto kSurface2 = ftxui::Color::RGB(0x58, 0x5b, 0x70);
const auto kMauve = ftxui::Color::RGB(0xcb, 0xa6, 0xf7);
const auto kLavender = ftxui::Color::RGB(0xb4, 0xbe, 0xfe);
const auto kBlue = ftxui::Color::RGB(0x89, 0xb4, 0xfa);
const auto kGreen = ftxui::Color::RGB(0xa6, 0xe3, 0xa1);
const auto kYellow = ftxui::Color::RGB(0xf9, 0xe2, 0xaf);
const auto kPeach = ftxui::Color::RGB(0xfa, 0xb3, 0x87);
const auto kRed = ftxui::Color::RGB(0xf3, 0x8b, 0xa8);
}  // namespace theme

namespace icon {
const std::string kAppLogo = "󰄛";     // Cat logo
const std::string kLibrary = "󰓹";     // Bookshelf
const std::string kInProgress = "󰐌";  // Play circle
const std::string kSearch = "";       // Search glass
const std::string kStats = "󰄫";       // Dashboard / chart
const std::string kSync = "󰓦";        // Sync arrows
const std::string kOffline = "";     // Offline icon

const std::string kAnime = "󰴙";  // Anime icon
const std::string kManga = "󱥬";  // Manga icon

const std::string kWatching = "󰐌";   // Play circle
const std::string kCompleted = "󰗠";  // Checked circle icon
const std::string kOnHold = "󰏥";     // Pause circle
const std::string kDropped = "󰅙";    // Dropped icon
const std::string kPlan = "󰥔";       // Clock outline

// Selection Checkbox Glyphs
const std::string kCheckSingle = "󰄲";  // Single-select checked icon
const std::string kCheckMulti = "󰄸";   // Multi-select checked icon
const std::string kCheckBlank = "󰄮";   // Unchecked outline

// 5-Star Glyphs
const std::string kStarFull = "";
const std::string kStarHalf = "";
const std::string kStarEmpty = "";

const std::string kGauge = "󰑋";
const std::string kKeyboard = "󰌌";
const std::string kWeb = "󰖟";
const std::string kWarn = "⚠️";
}  // namespace icon

}  // namespace catalist
