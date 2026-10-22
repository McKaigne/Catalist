#include "catalist/database.hpp"
#include "catalist/entry.hpp"
#include "catalist/malParser.hpp"
#include "catalist/cli.hpp"
#include "catalist/remoteSearch.hpp"
#include "catalist/config.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

#define TEST_ASSERT(cond) \
  do { \
    if (!(cond)) { \
      std::cerr << "\n[FAIL] Assertion failed at " << __FILE__ << ":" << __LINE__ \
                << "\n       Condition: " << #cond << std::endl; \
      std::exit(1); \
    } \
  } while (0)

static std::filesystem::path getTestSandbox() {
  auto p = std::filesystem::temp_directory_path() / "catalist_test_sandbox";
  std::filesystem::create_directories(p);
  return p;
}

void testCsvMultilineRoundTrip() {
  std::cout << "[RUN ] Testing RFC 4180 multiline CSV integrity..." << std::endl;
  catalist::Database db(getTestSandbox());

  std::filesystem::path tempCsv = getTestSandbox() / "test_multiline.csv";

  catalist::Entry original;
  original.id = 9901;
  original.malId = 9901;
  original.type = catalist::MediaType::Anime;
  original.title = "Fate/stay night: Heaven's Feel, Part \"1\"";
  original.status = catalist::WatchStatus::Completed;
  original.score = 10;
  original.progress = 3;
  original.totalUnits = 3;
  original.notes = "Line 1, with embedded comma.\nLine 2 with \"escaped quotes\".\nLine 3 final.";
  original.startDate = "2017-10-14";
  original.finishDate = "2020-08-15";

  db.addEntry(original);
  bool saved = db.saveCsv(catalist::MediaType::Anime, tempCsv);
  TEST_ASSERT(saved);

  catalist::Database reloadedDb(getTestSandbox());
  bool loaded = reloadedDb.loadCsv(catalist::MediaType::Anime, tempCsv);
  TEST_ASSERT(loaded);

  auto loadedOpt = reloadedDb.getById(catalist::MediaType::Anime, 9901);
  TEST_ASSERT(loadedOpt.has_value());

  const auto& e = loadedOpt.value();
  TEST_ASSERT(e.title == original.title);
  TEST_ASSERT(e.notes == original.notes);
  TEST_ASSERT(e.score == 10);
  TEST_ASSERT(e.progress == 3);
  TEST_ASSERT(e.startDate == "2017-10-14");
  TEST_ASSERT(e.finishDate == "2020-08-15");

  std::filesystem::remove(tempCsv);
  std::cout << "[PASS] RFC 4180 multiline CSV round-trip passed." << std::endl;
}

void testMalXmlIngestion() {
  std::cout << "[RUN ] Testing MAL XML ingestion with CDATA..." << std::endl;
  std::filesystem::path tempXml = getTestSandbox() / "test_mal.xml";

  std::ofstream xmlFile(tempXml);
  xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
          << "<myanimelist>\n"
          << "  <anime>\n"
          << "    <series_animedb_id>52991</series_animedb_id>\n"
          << "    <series_title><![CDATA[Sousou no Frieren, Special Edition]]></series_title>\n"
          << "    <series_episodes>28</series_episodes>\n"
          << "    <my_watched_episodes>28</my_watched_episodes>\n"
          << "    <my_score>10</my_score>\n"
          << "    <my_status>Completed</my_status>\n"
          << "    <my_start_date>2023-09-29</my_start_date>\n"
          << "    <my_finish_date>2024-03-22</my_finish_date>\n"
          << "    <my_comments><![CDATA[Masterpiece adaptation, with \"quotes\".]]></my_comments>\n"
          << "  </anime>\n"
          << "</myanimelist>\n";
  xmlFile.close();

  catalist::Database db(getTestSandbox());
  size_t imported = catalist::MalParser::importXml(tempXml.string(), db, true);
  TEST_ASSERT(imported == 1);

  auto entryOpt = db.getById(catalist::MediaType::Anime, 52991);
  TEST_ASSERT(entryOpt.has_value());
  TEST_ASSERT(entryOpt->title == "Sousou no Frieren, Special Edition");
  TEST_ASSERT(entryOpt->score == 10);
  TEST_ASSERT(entryOpt->progress == 28);
  TEST_ASSERT(entryOpt->totalUnits == 28);
  TEST_ASSERT(entryOpt->status == catalist::WatchStatus::Completed);
  TEST_ASSERT(entryOpt->notes == "Masterpiece adaptation, with \"quotes\".");

  std::filesystem::remove(tempXml);
  std::cout << "[PASS] MAL XML parsing and CDATA extraction passed." << std::endl;
}

void testSortAndFilter() {
  std::cout << "[RUN ] Testing Database sorting and filtering..." << std::endl;
  catalist::Database db(getTestSandbox());
  db.addEntry({52991, 52991, 154587, "Sousou no Frieren", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 10, 28, 28, 0, 0, 1, "2023-09-29", "2024-03-22", "Masterpiece adaptation.", "2026-10-17", false}); db.addEntry({49387, 49387, 136430, "Vinland Saga Season 2", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 9, 24, 24, 0, 0, 0, "2023-01-10", "2023-06-20", "Farmland arc.", "2026-10-17", false}); db.addEntry({457, 457, 457, "Mushishi", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 9, 26, 26, 0, 0, 0, "0000-00-00", "0000-00-00", "Atmospheric.", "2026-10-17", false}); db.addEntry({44265, 44265, 128547, "Odd Taxi", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 8, 13, 13, 0, 0, 0, "2021-04-06", "2021-06-29", "Original screenplay.", "2026-10-17", false}); db.addEntry({2, 2, 30002, "Berserk", catalist::MediaType::Manga, catalist::WatchStatus::Watching, 10, 364, 0, 42, 0, 0, "0000-00-00", "0000-00-00", "Dark fantasy pinnacle.", "2026-10-17", false}); db.addEntry({116778, 116778, 105778, "Chainsaw Man", catalist::MediaType::Manga, catalist::WatchStatus::Watching, 9, 217, 0, 18, 0, 0, "0000-00-00", "0000-00-00", "Part 2 ongoing.", "2026-10-17", false}); db.addEntry({135496, 135496, 132049, "Dandadan", catalist::MediaType::Manga, catalist::WatchStatus::Watching, 9, 167, 0, 16, 0, 0, "0000-00-00", "0000-00-00", "Peak art.", "2026-10-17", false}); db.addEntry({126146, 126146, 117195, "[Oshi no Ko]", catalist::MediaType::Manga, catalist::WatchStatus::Completed, 8, 166, 166, 16, 16, 0, "0000-00-00", "2024-11-14", "Entertainment industry.", "2026-10-17", false});

  auto byDate = db.filterEntries(catalist::MediaType::Anime, std::nullopt, -1, "", catalist::SortKey::Date, false);
  TEST_ASSERT(!byDate.empty());
  TEST_ASSERT(byDate[0].title == "Sousou no Frieren");

  auto byScore = db.filterEntries(catalist::MediaType::Anime, std::nullopt, -1, "", catalist::SortKey::Score, false);
  TEST_ASSERT(byScore[0].score >= byScore.back().score);

  auto searched = db.filterEntries(catalist::MediaType::Anime, std::nullopt, -1, "frieren", catalist::SortKey::Title, false);
  TEST_ASSERT(searched.size() == 1);
  TEST_ASSERT(searched[0].id == 52991);

  std::cout << "[PASS] Database sorting and filtering predicates passed." << std::endl;
}

void testCliParsers() {
  std::cout << "[RUN ] Testing CLI relational and range rating specs..." << std::endl;
  int minS = -1, maxS = -1;
  bool unrated = false;

  TEST_ASSERT(catalist::CliDispatcher::parseRatingSpec("9", minS, maxS, unrated) && minS == 9 && maxS == 9);
  TEST_ASSERT(catalist::CliDispatcher::parseRatingSpec(">=8", minS, maxS, unrated) && minS == 8 && maxS == 10);
  TEST_ASSERT(catalist::CliDispatcher::parseRatingSpec("<=6", minS, maxS, unrated) && minS == 0 && maxS == 6);
  TEST_ASSERT(catalist::CliDispatcher::parseRatingSpec("7..9", minS, maxS, unrated) && minS == 7 && maxS == 9);
  TEST_ASSERT(catalist::CliDispatcher::parseRatingSpec("unrated", minS, maxS, unrated) && unrated && minS == 0);

  catalist::Entry e;
  e.id = 42;
  e.title = "Odd Taxi";
  e.score = 8;
  std::string json = catalist::CliDispatcher::formatJsonEntry(e);
  TEST_ASSERT(json.find("\"id\":42") != std::string::npos);
  TEST_ASSERT(json.find("\"title\":\"Odd Taxi\"") != std::string::npos);
  TEST_ASSERT(json.find("\"rating\":8") != std::string::npos);

  std::cout << "[PASS] CLI argument and formatting tests passed." << std::endl;
}

void testCliMutations() {
  std::cout << "[RUN ] Testing CLI mutations (add, set with stepping, rm)..." << std::endl;
  catalist::Database db(getTestSandbox());

  std::vector<std::string> addArgs = {
    "Dungeon Meshi", "-t", "anime", "-s", "watching", "-e", "24", "-p", "10", "-r", "9"
  };
  TEST_ASSERT(catalist::CliDispatcher::executeAdd(addArgs, db) == 0);

  auto entries = db.filterEntries(catalist::MediaType::Anime, std::nullopt, -1, "Dungeon Meshi", catalist::SortKey::Title, false);
  TEST_ASSERT(entries.size() == 1);
  int id = entries[0].id;
  TEST_ASSERT(entries[0].progress == 10);
  TEST_ASSERT(entries[0].totalUnits == 24);
  TEST_ASSERT(entries[0].score == 9);

  std::vector<std::string> setStepArgs = {
    std::to_string(id), "-p", "a", "2"
  };
  TEST_ASSERT(catalist::CliDispatcher::executeSet(setStepArgs, db) == 0);
  auto eOpt = db.getById(catalist::MediaType::Anime, id);
  TEST_ASSERT(eOpt.has_value() && eOpt->progress == 12);

  std::vector<std::string> setSubArgs = {
    std::to_string(id), "-p", "sub", "2"
  };
  TEST_ASSERT(catalist::CliDispatcher::executeSet(setSubArgs, db) == 0);
  eOpt = db.getById(catalist::MediaType::Anime, id);
  TEST_ASSERT(eOpt.has_value() && eOpt->progress == 10);

  std::vector<std::string> setMaxArgs = {
    std::to_string(id), "-p", "24"
  };
  TEST_ASSERT(catalist::CliDispatcher::executeSet(setMaxArgs, db) == 0);
  eOpt = db.getById(catalist::MediaType::Anime, id);
  TEST_ASSERT(eOpt.has_value() && eOpt->progress == 24 && eOpt->status == catalist::WatchStatus::Completed);

  std::vector<std::string> rmArgs = {
    std::to_string(id), "-y"
  };
  TEST_ASSERT(catalist::CliDispatcher::executeRm(rmArgs, db) == 0);
  eOpt = db.getById(catalist::MediaType::Anime, id);
  TEST_ASSERT(!eOpt.has_value());

  std::cout << "[PASS] CLI mutations and progress stepping tests passed." << std::endl;
}

void testCliAnalyticsAndExport() {
  std::cout << "[RUN ] Testing CLI analytics (stats) and export (json, csv)..." << std::endl;
  catalist::Database db(getTestSandbox());
  db.addEntry({52991, 52991, 154587, "Sousou no Frieren", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 10, 28, 28, 0, 0, 1, "2023-09-29", "2024-03-22", "Masterpiece adaptation.", "2026-10-17", false}); db.addEntry({49387, 49387, 136430, "Vinland Saga Season 2", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 9, 24, 24, 0, 0, 0, "2023-01-10", "2023-06-20", "Farmland arc.", "2026-10-17", false}); db.addEntry({457, 457, 457, "Mushishi", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 9, 26, 26, 0, 0, 0, "0000-00-00", "0000-00-00", "Atmospheric.", "2026-10-17", false}); db.addEntry({44265, 44265, 128547, "Odd Taxi", catalist::MediaType::Anime, catalist::WatchStatus::Completed, 8, 13, 13, 0, 0, 0, "2021-04-06", "2021-06-29", "Original screenplay.", "2026-10-17", false}); db.addEntry({2, 2, 30002, "Berserk", catalist::MediaType::Manga, catalist::WatchStatus::Watching, 10, 364, 0, 42, 0, 0, "0000-00-00", "0000-00-00", "Dark fantasy pinnacle.", "2026-10-17", false}); db.addEntry({116778, 116778, 105778, "Chainsaw Man", catalist::MediaType::Manga, catalist::WatchStatus::Watching, 9, 217, 0, 18, 0, 0, "0000-00-00", "0000-00-00", "Part 2 ongoing.", "2026-10-17", false}); db.addEntry({135496, 135496, 132049, "Dandadan", catalist::MediaType::Manga, catalist::WatchStatus::Watching, 9, 167, 0, 16, 0, 0, "0000-00-00", "0000-00-00", "Peak art.", "2026-10-17", false}); db.addEntry({126146, 126146, 117195, "[Oshi no Ko]", catalist::MediaType::Manga, catalist::WatchStatus::Completed, 8, 166, 166, 16, 16, 0, "0000-00-00", "2024-11-14", "Entertainment industry.", "2026-10-17", false});

  TEST_ASSERT(catalist::CliDispatcher::executeStats({"--json"}, db) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeStats({"--type", "anime", "--json"}, db) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeExport({"json"}, db) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeExport({"json", "--type", "anime"}, db) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeExport({"csv"}, db) == 0);

  std::cout << "[PASS] CLI analytics and export tests passed." << std::endl;
}

void testCliConfigAndCompletions() {
  std::cout << "[RUN ] Testing CLI config and shell completion generators..." << std::endl;
  catalist::AppConfig config;

  TEST_ASSERT(catalist::CliDispatcher::executeConfig({"list"}, config) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeConfig({"get", "theme"}, config) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeCompletion({"bash"}) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeCompletion({"zsh"}) == 0);
  TEST_ASSERT(catalist::CliDispatcher::executeCompletion({"fish"}) == 0);

  std::cout << "[PASS] CLI config and completion tests passed." << std::endl;
}

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "   Catalist Headless Verification Suite  " << std::endl;
  std::cout << "========================================" << std::endl;

  testCsvMultilineRoundTrip();
  testMalXmlIngestion();
  testSortAndFilter();
  testCliParsers();
  testCliMutations();
  testCliAnalyticsAndExport();
  testCliConfigAndCompletions();

  std::cout << "========================================" << std::endl;
  std::cout << "   All automated tests passed (7/7)     " << std::endl;
  std::cout << "========================================" << std::endl;
  return 0;
}
