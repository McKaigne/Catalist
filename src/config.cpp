#include "catalist/config.hpp"
#include <fstream>
#include <sstream>

namespace catalist {

bool AppConfig::generateDefault(const std::filesystem::path& path) const {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  if (!file.is_open()) return false;

  file << "# ==============================================================================\n"
       << "# Catalist Configuration File\n"
       << "#\n"
       << "# This file uses Simplified Technical English (ASD-STE100).\n"
       << "# All parameters in this default file are comments.\n"
       << "# Remove the '#' character at the start of a line to activate a parameter.\n"
       << "# ==============================================================================\n\n"
       << "[general]\n"
       << "# Set the visual color theme.\n"
       << "# Approved values: \"catppuccin-mocha\", \"dark\", \"light\", \"tokyo-night\",\n"
       << "# \"kanagawa\", \"gruvbox\", \"nord\", \"dracula\", \"rose-pine\", \"ayu\",\n"
       << "# \"solarized-dark\", \"solarized-light\", \"custom\".\n"
       << "# Default: \"catppuccin-mocha\"\n"
       << "# theme = \"catppuccin-mocha\"\n\n"
       << "# Enable mouse control in the terminal interface.\n"
       << "# Default: true\n"
       << "# mouse = true\n\n"
       << "# Set the media view to show when the application starts.\n"
       << "# Approved values: \"anime\", \"manga\", \"overview\".\n"
       << "# Default: \"anime\"\n"
       << "# defaultMedia = \"anime\"\n\n"
       << "# Set the default sort column for the table.\n"
       << "# Approved values: \"score\", \"title\", \"date\", \"progress\", \"status\", \"id\".\n"
       << "# Default: \"score\"\n"
       << "# defaultSort = \"score\"\n\n"
       << "# Invert the sort direction.\n"
       << "# Default: true\n"
       << "# reverseSort = true\n\n"
       << "[keybinds]\n"
       << "# Navigation in list\n"
       << "# moveUp = \"k\"\n"
       << "# moveDown = \"j\"\n"
       << "# jumpTop = \"g\"\n"
       << "# jumpBottom = \"G\"\n\n"
       << "# Card focus controls\n"
       << "# focusLeft = \"h\"\n"
       << "# focusRight = \"l\"\n"
       << "# cyclePane = \"Tab\"\n\n"
       << "# Record changes\n"
       << "# increment = \"+\"\n"
       << "# decrement = \"-\"\n"
       << "# cycleStatus = \"Space\"\n\n"
       << "# Column sorting (press key to toggle between ascending and descending)\n"
       << "# sortDate = \"c\"\n"
       << "# sortTitle = \"t\"\n"
       << "# sortStatus = \"s\"\n"
       << "# sortRating = \"r\"\n"
       << "# sortProgress = \"p\"\n\n"
       << "# Media views\n"
       << "# tabAnime = \"1\"\n"
       << "# tabManga = \"2\"\n"
       << "# tabOverview = \"3\"\n\n"
       << "# Actions\n"
       << "# inspect = \"Enter\"\n"
       << "# editNotes = \"e\"\n"
       << "# rename = \"t\"\n"
       << "# add = \"a\"\n"
       << "# delete = \"d\"\n"
       << "# search = \"/\"\n"
       << "# openBrowser = \"o\"\n"
       << "# yank = \"y\"\n"
       << "# undo = \"u\"\n"
       << "# redo = \"U\"\n"
       << "# commandPalette = \":\"\n"
       << "# help = \"?\"\n"
       << "# quit = \"q\"\n\n"
       << "[theme.custom]\n"
       << "# Use hexadecimal color codes (#RRGGBB).\n"
       << "# These parameters apply only when theme is set to \"custom\".\n"
       << "# base = \"#1e1e2e\"\n"
       << "# mantle = \"#181825\"\n"
       << "# crust = \"#11111b\"\n"
       << "# surface0 = \"#313244\"\n"
       << "# surface1 = \"#45475a\"\n"
       << "# surface2 = \"#585b70\"\n"
       << "# text = \"#cdd6f4\"\n"
       << "# subtext = \"#a6adc8\"\n"
       << "# overlay0 = \"#6c7086\"\n"
       << "# mauve = \"#cba6f7\"\n"
       << "# lavender = \"#b4befe\"\n"
       << "# blue = \"#89b4fa\"\n"
       << "# green = \"#a6e3a1\"\n"
       << "# yellow = \"#f9e2af\"\n"
       << "# peach = \"#fab387\"\n"
       << "# red = \"#f38ba8\"\n";

  return true;
}

bool AppConfig::load(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    generateDefault(path);
    return false;
  }

  std::ifstream file(path);
  if (!file.is_open()) return false;

  std::string line;
  std::string section = "general";

  while (std::getline(file, line)) {
    auto start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) continue;
    if (line[start] == '#') continue;

    if (line[start] == '[') {
      auto end = line.find(']', start);
      if (end != std::string::npos) {
        section = line.substr(start + 1, end - start - 1);
      }
      continue;
    }

    auto eq = line.find('=', start);
    if (eq == std::string::npos) continue;

    std::string key = line.substr(start, eq - start);
    std::string val = line.substr(eq + 1);

    auto trim = [](std::string& s) {
      s.erase(0, s.find_first_not_of(" \t\r\n\""));
      s.erase(s.find_last_not_of(" \t\r\n\"") + 1);
    };
    trim(key);
    trim(val);

    if (section == "general") {
      if (key == "mouse") mouse = (val == "true" || val == "1");
      else if (key == "theme") theme = val;
      else if (key == "default_media" || key == "defaultMedia") defaultMedia = val;
      else if (key == "default_sort" || key == "defaultSort") defaultSort = val;
      else if (key == "reverse_sort" || key == "reverseSort") reverseSort = (val == "true" || val == "1");
    } else if (section == "keybinds") {
      keybinds.setBinding(key, val);
    } else if (section == "theme.custom") {
      auto c = ThemeRegistry::hexToColor(val);
      if (key == "base") customPalette.base = c;
      else if (key == "mantle") customPalette.mantle = c;
      else if (key == "crust") customPalette.crust = c;
      else if (key == "surface0") customPalette.surface0 = c;
      else if (key == "surface1") customPalette.surface1 = c;
      else if (key == "surface2") customPalette.surface2 = c;
      else if (key == "text") customPalette.text = c;
      else if (key == "subtext") customPalette.subtext = c;
      else if (key == "overlay0") customPalette.overlay0 = c;
      else if (key == "mauve") customPalette.mauve = c;
      else if (key == "lavender") customPalette.lavender = c;
      else if (key == "blue") customPalette.blue = c;
      else if (key == "green") customPalette.green = c;
      else if (key == "yellow") customPalette.yellow = c;
      else if (key == "peach") customPalette.peach = c;
      else if (key == "red") customPalette.red = c;
    }
  }

  if (theme == "custom") {
    ThemeRegistry::setCustomPalette(customPalette);
  }
  ThemeRegistry::setActive(theme);
  return true;
}

bool AppConfig::save(const std::filesystem::path& path) const {
  std::ofstream file(path);
  if (!file.is_open()) return false;

  file << "# Catalist Configuration File\n"
       << "[general]\n"
       << "mouse = " << (mouse ? "true" : "false") << "\n"
       << "theme = \"" << theme << "\"\n"
       << "defaultMedia = \"" << defaultMedia << "\"\n"
       << "defaultSort = \"" << defaultSort << "\"\n"
       << "reverseSort = " << (reverseSort ? "true" : "false") << "\n";
  return true;
}

} // namespace catalist
