#include "catalist/database.hpp"
#include "catalist/entry.hpp"
#include "catalist/malParser.hpp"

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

void testCsvMultilineRoundTrip() {
  std::cout << "[RUN ] Testing RFC 4180 multiline CSV integrity..." << std::endl;
  catalist::Database db;

  std::filesystem::path tempCsv = std::filesystem::temp_directory_path() / "catalist_test_multiline.csv";

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

  catalist::Database reloadedDb;
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
  std::filesystem::path tempXml = std::filesystem::temp_directory_path() / "catalist_test_mal.xml";

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

  catalist::Database db;
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
  catalist::Database db;
  db.seedDefaults();

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

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "   Catalist Headless Verification Suite  " << std::endl;
  std::cout << "========================================" << std::endl;

  testCsvMultilineRoundTrip();
  testMalXmlIngestion();
  testSortAndFilter();

  std::cout << "========================================" << std::endl;
  std::cout << "   All automated tests passed (3/3)     " << std::endl;
  std::cout << "========================================" << std::endl;
  return 0;
}
