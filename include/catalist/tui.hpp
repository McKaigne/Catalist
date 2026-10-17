#pragma once

#include "catalist/database.hpp"
#include "catalist/config.hpp"
#include <string>
#include <vector>

namespace catalist {

enum class ViewTab {
  Anime,
  Manga,
  Overview
};

enum class ActiveTier {
  Sidebar,
  Catalog,
  CellEdit,
  Inspector,
  Search
};

enum class EditField {
  Status,
  Rating,
  Progress
};

struct HistoryAction {
  enum Type { Mutate, Delete, Add } type;
  MediaType media;
  Entry before;
  Entry after;
};

class AppTui {
public:
  AppTui(Database& db, AppConfig& config);
  void run();

private:
  Database& db_;
  AppConfig& config_;
  std::vector<HistoryAction> undoStack_;
  std::vector<HistoryAction> redoStack_;
};

} // namespace catalist
