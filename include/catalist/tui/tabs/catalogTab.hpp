#ifndef CATALIST_TUI_TABS_CATALOG_TAB_HPP_
#define CATALIST_TUI_TABS_CATALOG_TAB_HPP_

#include <ftxui/component/component.hpp>
#include <optional>
#include <string>
#include <vector>

#include "catalist/net/jikan.hpp"
#include "catalist/tui/tab.hpp"
#include "catalist/tui/widgets/marquee.hpp"

namespace catalist {

enum class focusedCard { kSidebar, kCatalog, kDetails };

enum class detailsField { kStatus, kRating, kProgress, kNotes };

// Where the add flow is: typing a title, waiting for the search, or picking
// one of the matches.
enum class addStage { kOff, kTyping, kSearching, kPicking };

// One sidebar row: a section header, "all", a status filter or a score filter.
struct sidebarRow {
  std::string label;
  bool isHeader = false;
  std::optional<watchStatus> status;
  std::optional<int> score;
};

// The anime or manga catalogue: sidebar filters, searchable table, details
// card, and adding titles found on MyAnimeList.
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
  ftxui::Component focusRoot() override { return root_; }
  [[nodiscard]] bool capturesInput() const override;
  [[nodiscard]] appMode mode() const override;
  [[nodiscard]] std::optional<std::string> countBadge(
      appContext& ctx) const override;

  // The rows of the table, in the order shown.
  [[nodiscard]] const std::vector<entry>& shown() const { return displayed_; }

private:
  void refresh(appContext& ctx);
  [[nodiscard]] bool passesFilters(const entry& candidate) const;
  [[nodiscard]] const entry* selectedEntry() const;
  void selectId(int id);
  void returnToCatalog();

  bool handlePendingDelete(const ftxui::Event& event, appContext& ctx);
  bool handleSearchTyping(const ftxui::Event& event, appContext& ctx);
  bool handleNotesEditing(const ftxui::Event& event, appContext& ctx);
  bool handleAdding(const ftxui::Event& event, appContext& ctx);
  bool handleEntryKeys(const ftxui::Event& event, appContext& ctx);
  bool handleSortKeys(const ftxui::Event& event, appContext& ctx);
  bool handleCatalogFocus(const ftxui::Event& event, appContext& ctx);
  bool handleSidebarFocus(const ftxui::Event& event, appContext& ctx);
  bool handleDetailsFocus(const ftxui::Event& event, appContext& ctx);
  bool handleCommands(const ftxui::Event& event, appContext& ctx);

  void commit(appContext& ctx, const entry& changed, const std::string& done);
  void adjustDetailsField(appContext& ctx, int direction);
  void toggleSort(sortKey key, const std::string& name, appContext& ctx);
  void moveSidebarCursor(int direction, appContext& ctx);
  void startSearch(appContext& ctx);
  void commitNotes(appContext& ctx);
  void requestDelete(appContext& ctx);
  void confirmDelete(appContext& ctx);
  void startAdding(appContext& ctx);
  void cancelAdding(appContext& ctx);
  void addPicked(appContext& ctx);

  ftxui::Element renderSidebarRow(int row) const;
  ftxui::Element renderCatalogRow(int row);
  ftxui::Element renderHeaderCell(const std::string& name, sortKey key,
                                  int width, bool stretches) const;
  ftxui::Element renderDetails();
  ftxui::Element renderSidebarCard() const;
  ftxui::Element renderSearchCard();
  ftxui::Element renderCatalogCard();
  ftxui::Element renderPickCard();

  mediaType media_;
  appMode mode_ = appMode::kNor;
  focusedCard focus_ = focusedCard::kCatalog;
  detailsField detailsCursor_ = detailsField::kStatus;
  sortKey sortBy_ = sortKey::kScore;
  bool reverseSort_ = false;

  int sidebarSelected_ = 1;
  int catalogSelected_ = 0;
  std::string searchQuery_;
  std::string notesBuffer_;
  int notesCursor_ = 0;
  bool editingNotes_ = false;
  std::optional<int> pendingDeleteId_;

  addStage adding_ = addStage::kOff;
  std::string addQuery_;
  std::vector<jikan::title> addResults_;
  int addSelected_ = 0;

  std::vector<sidebarRow> sidebarRows_;
  std::vector<entry> displayed_;
  bool libraryEmpty_ = true;

  marquee::selectionTimer rowTimer_;
  marquee::selectionTimer detailsTimer_;

  // Only the text fields are components, so FTXUI draws their cursor; every
  // key reaches them through onEvent.
  ftxui::Component idle_;
  ftxui::Component searchInput_;
  ftxui::Component addInput_;
  ftxui::Component notesInput_;
  ftxui::Component root_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TABS_CATALOG_TAB_HPP_
