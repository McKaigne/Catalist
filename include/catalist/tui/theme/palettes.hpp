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
const std::string kAppLogo = "󰄛";
const std::string kLibrary = "󰓹";
const std::string kInProgress = "󰐌";
const std::string kSearch = "";
const std::string kStats = "󰄫";
const std::string kSync = "󰓦";
const std::string kOffline = "";

const std::string kAnime = "󰴙";
const std::string kManga = "󱥬";

const std::string kWatching = "󰐌";
const std::string kCompleted = "󰗠";
const std::string kOnHold = "󰏥";
const std::string kDropped = "󰅙";
const std::string kPlan = "󰥔";

const std::string kCheckSingle = "󰄲";
const std::string kCheckMulti = "󰄸";
const std::string kCheckBlank = "󰄮";

const std::string kStarFull = "";
const std::string kStarHalf = "";
const std::string kStarEmpty = "";

const std::string kGauge = "󰑋";
const std::string kKeyboard = "󰌌";
const std::string kWeb = "󰖟";
const std::string kWarn = "⚠️";
}  // namespace icon

}  // namespace catalist
