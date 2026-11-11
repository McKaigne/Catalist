#ifndef CATALIST_TUI_CATALOG_TAB_HPP_
#define CATALIST_TUI_CATALOG_TAB_HPP_

#include <array>
#include <ftxui/screen/box.hpp>
#include <optional>
#include <string>
#include <vector>

#include "jikan.hpp"
#include "tui/app.hpp"
#include "tui/style.hpp"

namespace catalist {

enum class focusedCard { kSidebar, kCatalog, kDetails };

// The rows of the details card the cursor can stop on. Volumes are shown for
// manga only.
enum class detailsField {
  kStatus,
  kRating,
  kProgress,
  kTotal,
  kVolumes,
  kRewatched
};

// What the keyboard is doing in the tab. Every step but browsing owns the
// keyboard until it ends.
enum class step {
  kBrowse,
  kSearch,         // typing a filter
  kTypeNumber,     // typing a count in the details card
  kAddTyping,      // typing a title to add
  kAddSearching,   // waiting for the MyAnimeList search
  kAddPicking,     // choosing one of its matches
  kConfirmDelete,  // y/n for the selected entry
};

// The numbers under the sidebar, worked out again on every refresh.
struct libraryTotals {
  double daysSpent = 0.0;
  double meanScore = 0.0;
  int units = 0;  // episodes or chapters
  int rewatched = 0;
  int volumes = 0;
  // Titles per status, in watchStatus order, for the status bar.
  std::array<size_t, 5> byStatus{};
  size_t titles = 0;
};

// One sidebar row: a section header, "all", a status filter or a score filter.
struct sidebarRow {
  std::string label;
  bool isHeader = false;
  std::optional<watchStatus> status;
  std::optional<int> score;
};

// The anime or manga catalogue: sidebar filters and totals, searchable table,
// details card, and adding titles found on MyAnimeList. Keys and state live in
// catalogTab.cpp, drawing in catalogView.cpp.
class catalogTab : public iTab {
public:
  explicit catalogTab(mediaType media);

  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;

  void onActivate(appContext& ctx) override;
  void onTick(appContext& ctx) override;
  [[nodiscard]] appMode mode() const override;
  [[nodiscard]] std::optional<std::string> countBadge(
      appContext& ctx) const override;
  [[nodiscard]] std::optional<overlayImage> image() const override;

  // The rows of the table, in the order shown.
  [[nodiscard]] const std::vector<entry>& shown() const { return displayed_; }

private:
  // State.
  void refresh(appContext& ctx);
  [[nodiscard]] bool passesFilters(const entry& candidate) const;
  [[nodiscard]] const entry* selectedEntry() const;
  void selectId(int id);
  void browse();

  // Keys, one handler per step; browsing splits by the focused card.
  bool onBrowseKey(const ftxui::Event& event, appContext& ctx);
  bool onSearchKey(const ftxui::Event& event, appContext& ctx);
  bool onNumberKey(const ftxui::Event& event, appContext& ctx);
  bool onAddTitleKey(const ftxui::Event& event, appContext& ctx);
  bool onPickKey(const ftxui::Event& event, appContext& ctx);
  bool onConfirmDeleteKey(const ftxui::Event& event, appContext& ctx);
  bool onEntryKey(const ftxui::Event& event, appContext& ctx);
  bool onSortKey(const ftxui::Event& event, appContext& ctx);
  bool onCatalogKey(const ftxui::Event& event, appContext& ctx);
  bool onSidebarKey(const ftxui::Event& event, appContext& ctx);
  bool onDetailsKey(const ftxui::Event& event, appContext& ctx);

  // Actions.
  void commit(appContext& ctx, const entry& changed, const std::string& done);
  void adjustDetailsField(appContext& ctx, int direction);
  void toggleSort(sortKey key, const std::string& name, appContext& ctx);
  void moveSidebarCursor(int direction, appContext& ctx);
  void moveDetailsCursor(int direction);
  void commitNumber(appContext& ctx);
  void addPicked(appContext& ctx);
  [[nodiscard]] std::vector<detailsField> detailsFields() const;
  [[nodiscard]] std::string fieldLabel(detailsField field) const;

  // Drawing (catalogView.cpp).
  ftxui::Element renderSidebarRow(int row) const;
  ftxui::Element renderCatalogRow(int row);
  ftxui::Element renderHeaderCell(const std::string& name, sortKey key,
                                  int width, bool stretches) const;
  ftxui::Element renderDetails();
  ftxui::Element renderSidebarCard() const;
  ftxui::Element renderStatsCard() const;
  ftxui::Element renderSearchCard() const;
  ftxui::Element renderCatalogCard();
  ftxui::Element renderPickCard();
  ftxui::Element renderCoverCard(appContext& ctx);

  // Text both halves need.
  static bool isSortAscending(sortKey key, bool reverse);
  static std::string progressLabel(const entry& e);
  static std::string unitName(mediaType type);
  static std::optional<countField> countFieldOf(detailsField field);
  static std::string countLabel(const entry& e, countField field);

  [[nodiscard]] bool isAdding() const;

  mediaType media_;
  step step_ = step::kBrowse;
  focusedCard focus_ = focusedCard::kCatalog;
  detailsField detailsCursor_ = detailsField::kStatus;
  sortKey sortBy_ = sortKey::kNew;
  bool reverseSort_ = false;

  int sidebarSelected_ = 1;
  int catalogSelected_ = 0;
  std::vector<sidebarRow> sidebarRows_;
  std::vector<entry> displayed_;
  bool libraryEmpty_ = true;
  libraryTotals totals_;

  textField search_;
  textField number_;
  textField addTitle_;
  std::vector<jikan::title> addResults_;
  int addSelected_ = 0;

  marquee::selectionTimer rowTimer_;
  marquee::selectionTimer detailsTimer_;
  // Where the cover card's inside was drawn last, and the picture for it.
  ftxui::Box coverBox_;
  std::optional<overlayImage> cover_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_CATALOG_TAB_HPP_
