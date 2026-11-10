#include <doctest/doctest.h>

#include "catalist/storage/malXml.hpp"
#include "support/testSupport.hpp"

using catalist::mediaType;
using catalist::watchStatus;
namespace malXml = catalist::malXml;

TEST_CASE("malXml reads an anime block the way MyAnimeList writes it") {
  auto titles = malXml::parse(
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<myanimelist>\n"
      "  <myinfo><user_export_type>1</user_export_type></myinfo>\n"
      "  <anime>\n"
      "    <series_animedb_id>52991</series_animedb_id>\n"
      "    <series_title><![CDATA[Sousou no Frieren]]></series_title>\n"
      "    <series_type>TV</series_type>\n"
      "    <series_episodes>28</series_episodes>\n"
      "    <my_watched_episodes>27</my_watched_episodes>\n"
      "    <my_start_date>2023-09-29</my_start_date>\n"
      "    <my_finish_date>0000-00-00</my_finish_date>\n"
      "    <my_score>10</my_score>\n"
      "    <my_status>Watching</my_status>\n"
      "    <my_comments><![CDATA[Slow & good.]]></my_comments>\n"
      "    <my_times_watched>2</my_times_watched>\n"
      "  </anime>\n"
      "</myanimelist>\n");

  REQUIRE(titles.size() == 1);
  const auto& frieren = titles[0];
  CHECK(frieren.type == mediaType::kAnime);
  CHECK(frieren.id == 0);
  CHECK(frieren.malId == 52991);
  CHECK(frieren.title == "Sousou no Frieren");
  CHECK(frieren.totalUnits == 28);
  CHECK(frieren.progress == 27);
  CHECK(frieren.rewatched == 2);
  CHECK(frieren.score == 10);
  CHECK(frieren.status == watchStatus::kWatching);
  CHECK(catalist::formatDate(frieren.startDate) == "2023-09-29");
  CHECK_FALSE(frieren.finishDate.has_value());
  CHECK(frieren.notes == "Slow & good.");
  CHECK(frieren.volumes == 0);
  CHECK(frieren.totalVolumes == 0);
}

TEST_CASE("malXml reads a manga block with chapters and volumes") {
  auto titles = malXml::parse(
      "<myanimelist>\n"
      "  <manga>\n"
      "    <manga_mangadb_id>2</manga_mangadb_id>\n"
      "    <manga_title><![CDATA[Berserk]]></manga_title>\n"
      "    <manga_volumes>41</manga_volumes>\n"
      "    <manga_chapters>380</manga_chapters>\n"
      "    <my_read_volumes>40</my_read_volumes>\n"
      "    <my_read_chapters>364</my_read_chapters>\n"
      "    <my_times_read>2</my_times_read>\n"
      "    <my_score>9</my_score>\n"
      "    <my_status>Reading</my_status>\n"
      "  </manga>\n"
      "</myanimelist>\n");

  REQUIRE(titles.size() == 1);
  const auto& berserk = titles[0];
  CHECK(berserk.type == mediaType::kManga);
  CHECK(berserk.malId == 2);
  CHECK(berserk.title == "Berserk");
  CHECK(berserk.totalUnits == 380);
  CHECK(berserk.progress == 364);
  CHECK(berserk.totalVolumes == 41);
  CHECK(berserk.volumes == 40);
  CHECK(berserk.rewatched == 2);
  CHECK(berserk.score == 9);
  CHECK(berserk.status == watchStatus::kWatching);
}

TEST_CASE("malXml reads every block, anime before manga") {
  auto titles = malXml::parse(
      "<manga><manga_mangadb_id>2</manga_mangadb_id>"
      "<manga_title>Berserk</manga_title></manga>"
      "<anime><series_animedb_id>1</series_animedb_id>"
      "<series_title>Cowboy Bebop</series_title></anime>"
      "<anime><series_animedb_id>5</series_animedb_id>"
      "<series_title>Monster</series_title></anime>");
  REQUIRE(titles.size() == 3);
  CHECK(titles[0].title == "Cowboy Bebop");
  CHECK(titles[1].title == "Monster");
  CHECK(titles[2].title == "Berserk");
}

TEST_CASE("malXml maps each MyAnimeList status") {
  auto statusOf = [](const std::string& text) {
    auto titles = malXml::parse("<anime><series_title>T</series_title>"
                                "<my_status>" + text + "</my_status></anime>");
    REQUIRE(titles.size() == 1);
    return titles[0].status;
  };
  CHECK(statusOf("Watching") == watchStatus::kWatching);
  CHECK(statusOf("Reading") == watchStatus::kWatching);
  CHECK(statusOf("Completed") == watchStatus::kCompleted);
  CHECK(statusOf("On-Hold") == watchStatus::kOnHold);
  CHECK(statusOf("Dropped") == watchStatus::kDropped);
  CHECK(statusOf("Plan to Watch") == watchStatus::kPlanToWatch);
  CHECK(statusOf("Plan to Read") == watchStatus::kPlanToWatch);
  CHECK(statusOf("Rewatching?") == watchStatus::kWatching);
  CHECK(statusOf("nonsense") == watchStatus::kPlanToWatch);
  CHECK(statusOf("") == watchStatus::kPlanToWatch);
}

TEST_CASE("malXml decodes entities outside CDATA and leaves CDATA alone") {
  auto titles = malXml::parse(
      "<anime>\n"
      "  <series_title>  Tom &amp; Jerry &lt;3 &quot;x&quot; &#039;y&apos; "
      "&amp;lt;  </series_title>\n"
      "  <my_comments><![CDATA[ a &amp; b ]]></my_comments>\n"
      "</anime>\n");
  REQUIRE(titles.size() == 1);
  CHECK(titles[0].title == "Tom & Jerry <3 \"x\" 'y' &lt;");
  CHECK(titles[0].notes == " a &amp; b ");
}

TEST_CASE("malXml keeps bad numbers and dates out of the catalogue") {
  auto titles = malXml::parse(
      "<anime><series_title>Odd</series_title>"
      "<series_animedb_id>abc</series_animedb_id>"
      "<series_episodes>-3</series_episodes>"
      "<my_watched_episodes>99999999999999999999</my_watched_episodes>"
      "<my_times_watched>1.5</my_times_watched>"
      "<my_score>42</my_score>"
      "<my_start_date>2023-02-30</my_start_date>"
      "<my_finish_date>soon</my_finish_date></anime>");
  REQUIRE(titles.size() == 1);
  CHECK(titles[0].malId == 0);
  CHECK(titles[0].totalUnits == 0);
  CHECK(titles[0].progress == 0);
  CHECK(titles[0].rewatched == 0);
  CHECK(titles[0].score == 10);
  CHECK_FALSE(titles[0].startDate.has_value());
  CHECK_FALSE(titles[0].finishDate.has_value());
}

TEST_CASE("malXml skips untitled and unterminated blocks") {
  CHECK(malXml::parse("").empty());
  CHECK(malXml::parse("not xml at all").empty());
  CHECK(malXml::parse("<anime><series_animedb_id>1</series_animedb_id></anime>")
            .empty());
  CHECK(malXml::parse("<anime><series_title>Cut off</series_title>").empty());
}

TEST_CASE("malXml writes one media type and leaves out titles without an id") {
  using catalist::test::makeEntry;
  std::vector<catalist::entry> entries = {
      makeEntry().malId(52991).title("Sousou no Frieren").totalUnits(28)
          .progress(28).rewatched(1).score(10)
          .status(watchStatus::kCompleted).startDate("2023-09-29")
          .finishDate("2024-03-22").notes("Great"),
      makeEntry().title("Home video"),
      makeEntry().malId(2).title("Berserk").type(mediaType::kManga),
  };

  auto anime = malXml::write(entries, mediaType::kAnime);
  CHECK(anime.written == 1);
  CHECK(anime.skipped == 1);
  CHECK(anime.xml ==
        "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
        "<myanimelist>\n"
        "\t<myinfo>\n"
        "\t\t<user_export_type>1</user_export_type>\n"
        "\t</myinfo>\n"
        "\t<anime>\n"
        "\t\t<series_animedb_id>52991</series_animedb_id>\n"
        "\t\t<series_title><![CDATA[Sousou no Frieren]]></series_title>\n"
        "\t\t<series_episodes>28</series_episodes>\n"
        "\t\t<my_watched_episodes>28</my_watched_episodes>\n"
        "\t\t<my_times_watched>1</my_times_watched>\n"
        "\t\t<my_start_date>2023-09-29</my_start_date>\n"
        "\t\t<my_finish_date>2024-03-22</my_finish_date>\n"
        "\t\t<my_score>10</my_score>\n"
        "\t\t<my_status>Completed</my_status>\n"
        "\t\t<my_comments><![CDATA[Great]]></my_comments>\n"
        "\t\t<update_on_import>1</update_on_import>\n"
        "\t</anime>\n"
        "</myanimelist>\n");

  auto manga = malXml::write(entries, mediaType::kManga);
  CHECK(manga.written == 1);
  CHECK(manga.skipped == 0);
  CHECK(manga.xml.find("<user_export_type>2</user_export_type>") !=
        std::string::npos);
  CHECK(manga.xml.find("<manga_mangadb_id>2</manga_mangadb_id>") !=
        std::string::npos);
  CHECK(manga.xml.find("<my_status>Reading</my_status>") != std::string::npos);
  CHECK(manga.xml.find("Frieren") == std::string::npos);
}

TEST_CASE("malXml reads back exactly what it wrote") {
  using catalist::test::makeEntry;
  std::vector<catalist::entry> entries = {
      makeEntry().malId(1).title("A <b> & ]]> \"title\"").totalUnits(12)
          .progress(7).rewatched(3).score(8).status(watchStatus::kOnHold)
          .startDate("2020-01-02").notes("line one\nline ]]> two"),
      makeEntry().malId(9).title("Planned").status(watchStatus::kPlanToWatch),
      makeEntry().malId(2).title("Berserk").type(mediaType::kManga)
          .totalUnits(380).progress(364).totalVolumes(41).volumes(40)
          .rewatched(2).score(10).status(watchStatus::kDropped)
          .finishDate("2021-12-31"),
  };

  auto readBack = malXml::parse(malXml::write(entries, mediaType::kAnime).xml +
                                malXml::write(entries, mediaType::kManga).xml);
  REQUIRE(readBack.size() == entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    CAPTURE(i);
    CHECK(readBack[i].type == entries[i].type);
    CHECK(readBack[i].malId == entries[i].malId);
    CHECK(readBack[i].title == entries[i].title);
    CHECK(readBack[i].status == entries[i].status);
    CHECK(readBack[i].score == entries[i].score);
    CHECK(readBack[i].progress == entries[i].progress);
    CHECK(readBack[i].totalUnits == entries[i].totalUnits);
    CHECK(readBack[i].volumes == entries[i].volumes);
    CHECK(readBack[i].totalVolumes == entries[i].totalVolumes);
    CHECK(readBack[i].rewatched == entries[i].rewatched);
    CHECK(readBack[i].startDate == entries[i].startDate);
    CHECK(readBack[i].finishDate == entries[i].finishDate);
    CHECK(readBack[i].notes == entries[i].notes);
  }
}
