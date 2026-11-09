#ifndef CATALIST_TUI_TABS_CATALOG_TAB_HPP_
#define CATALIST_TUI_TABS_CATALOG_TAB_HPP_

#include <ftxui/component/component.hpp>
#include <ftxui/screen/box.hpp>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "catalist/tui/tab.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

enum class appMode { kNor, kSch, kEdt, kVis };

enum class focusedCard { kSidebar, kCatalog, kDetails };

enum class detailsField { kStatus, kRating, kProgress, kNotes };

// One sidebar row: a section header, "all", a status filter or a score filter.
struct sidebarRow {
  std::string label;
  bool isHeader = false;
  std::optional<watchStatus> status;
  std::optional<int> score;
};

// The anime or manga catalogue: sidebar filters, searchable table, details
// card and visual (batch) selection.
class catalogTab : public iTab {
public:
  catalogTab(mediaType media, const appSettings& settings);

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] std::string title() const override;
  [[nodiscard]] std::string icon() const override;

  ftxui::Element render(appContext& ctx) override;
  bool onEvent(const ftxui::Event& event, appContext& ctx) override;
  [[nodiscard]] std::vector<hint> hints() const override;

  void onActivate(appContext& ctx) override;
  ftxui::Component focusRoot() override { return root_; }
  [[nodiscard]] bool capturesInput() const override;
  [[nodiscard]] std::optional<std::string> countBadge(
      appContext& ctx) const override;
  [[nodiscard]] std::string selectionSummary() const override;

private:
  using entryChange = std::function<entry(const entry&, const isoDate&)>;

  void buildComponents();
  void refresh(appContext& ctx);
  [[nodiscard]] bool passesFilters(const entry& candidate) const;
  [[nodiscard]] const entry* selectedEntry() const;
  [[nodiscard]] bool isRowFilterActive(int row) const;

  bool handleMouse(const ftxui::Event& event, appContext& ctx);
  bool handlePendingDelete(const ftxui::Event& event, appContext& ctx);
  bool handleSearchTyping(const ftxui::Event& event, appContext& ctx);
  bool handleNotesEditing(const ftxui::Event& event, appContext& ctx);
  bool handleBatchKeys(const ftxui::Event& event, appContext& ctx);
  bool handleSortKeys(const ftxui::Event& event, appContext& ctx);
  bool handleCatalogFocus(const ftxui::Event& event, appContext& ctx);
  bool handleSidebarFocus(const ftxui::Event& event, appContext& ctx);
  bool handleDetailsFocus(const ftxui::Event& event, appContext& ctx);
  bool handleEntryCommands(const ftxui::Event& event, appContext& ctx);

  void applyChange(appContext& ctx, const entry& before,
                   const entryChange& change, const std::string& status);
  void applyToTargets(appContext& ctx, const entryChange& change,
                      const std::string& batchVerb);
  void adjustDetailsField(appContext& ctx, int direction);
  void toggleSort(sortKey key, const std::string& name, appContext& ctx);
  void toggleVisualSelection();
  void toggleSidebarMultiFilter();
  void moveSidebarCursor(int direction, appContext& ctx);
  void startSearch(appContext& ctx);
  void commitNotes(appContext& ctx);
  void requestDelete(appContext& ctx);
  void confirmDelete(appContext& ctx);
  void undoLast(appContext& ctx);
  void redoLast(appContext& ctx);

  ftxui::Element renderSidebarRow(int row) const;
  ftxui::Element renderCatalogRow(int row);
  ftxui::Element renderHeaderCell(const std::string& name, sortKey key,
                                  int width, ftxui::Box& box,
                                  bool stretches) const;
  ftxui::Element renderDetails(appContext& ctx);
  ftxui::Element renderSidebarCard() const;
  ftxui::Element renderSearchCard();
  ftxui::Element renderCatalogCard();

  mediaType media_;
  appMode mode_ = appMode::kNor;
  focusedCard focus_ = focusedCard::kCatalog;
  detailsField detailsCursor_ = detailsField::kStatus;
  sortKey sortBy_ = sortKey::kScore;
  bool reverseSort_ = false;

  int sidebarSelected_ = 1;
  int catalogSelected_ = 0;
  std::set<watchStatus> statusFilters_;
  std::set<int> scoreFilters_;
  std::set<int> visualSelectedIds_;
  std::string searchQuery_;
  std::string notesBuffer_;
  bool editingNotes_ = false;
  std::optional<int> pendingDeleteId_;

  std::vector<sidebarRow> sidebarRows_;
  std::vector<std::string> sidebarLabels_;
  std::vector<std::string> catalogLabels_;
  std::vector<entry> displayed_;

  ftxui::Box headerDateBox_, headerTitleBox_, headerStatusBox_,
      headerRatingBox_, headerProgressBox_, searchBarBox_;
  marquee::selectionTimer rowTimer_;
  marquee::selectionTimer detailsTimer_;

  ftxui::Component sidebarMenu_;
  ftxui::Component searchInput_;
  ftxui::Component catalogMenu_;
  ftxui::Component notesInput_;
  ftxui::Component root_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_CATALOG_TAB_HPP_
