#include "storage/mal_xml.hpp"

#include <doctest/doctest.h>

#include <format>

#include "support/test_support.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
namespace mal_xml = catalist::mal_xml;

TEST_CASE("malXml reads an anime block the way MyAnimeList writes it") {
	auto titles = mal_xml::Parse(
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
	    "    <my_comments><![CDATA[Not kept.]]></my_comments>\n"
	    "    <my_times_watched>2</my_times_watched>\n"
	    "  </anime>\n"
	    "</myanimelist>\n");

	REQUIRE(titles.size() == 1);
	const auto& frieren = titles[0];
	CHECK(frieren.type == MediaType::kAnime);
	CHECK(frieren.id == 0);
	CHECK(frieren.mal_id == 52991);
	CHECK(frieren.title == "Sousou no Frieren");
	CHECK(frieren.total_units == 28);
	CHECK(frieren.progress == 27);
	CHECK(frieren.rewatched == 2);
	CHECK(frieren.score == 10);
	CHECK(frieren.status == WatchStatus::kWatching);
	CHECK(catalist::FormatDate(frieren.start_date) == "2023-09-29");
	CHECK_FALSE(frieren.finish_date.has_value());
	CHECK(frieren.volumes == 0);
	CHECK(frieren.total_volumes == 0);
}

TEST_CASE("malXml reads a manga block with chapters and volumes") {
	auto titles = mal_xml::Parse(
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
	CHECK(berserk.type == MediaType::kManga);
	CHECK(berserk.mal_id == 2);
	CHECK(berserk.title == "Berserk");
	CHECK(berserk.total_units == 380);
	CHECK(berserk.progress == 364);
	CHECK(berserk.total_volumes == 41);
	CHECK(berserk.volumes == 40);
	CHECK(berserk.rewatched == 2);
	CHECK(berserk.score == 9);
	CHECK(berserk.status == WatchStatus::kWatching);
}

TEST_CASE("malXml reads every block, anime before manga") {
	auto titles = mal_xml::Parse(
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
	auto status_of = [](const std::string& text) {
		auto titles = mal_xml::Parse(
		    "<anime><series_title>T</series_title>"
		    "<my_status>" +
		    text + "</my_status></anime>");
		REQUIRE(titles.size() == 1);
		return titles[0].status;
	};
	CHECK(status_of("Watching") == WatchStatus::kWatching);
	CHECK(status_of("Reading") == WatchStatus::kWatching);
	CHECK(status_of("Completed") == WatchStatus::kCompleted);
	CHECK(status_of("On-Hold") == WatchStatus::kOnHold);
	CHECK(status_of("Dropped") == WatchStatus::kDropped);
	CHECK(status_of("Plan to Watch") == WatchStatus::kPlanToWatch);
	CHECK(status_of("Plan to Read") == WatchStatus::kPlanToWatch);
	CHECK(status_of("Rewatching?") == WatchStatus::kRewatching);
	CHECK(status_of("nonsense") == WatchStatus::kPlanToWatch);
	CHECK(status_of("") == WatchStatus::kPlanToWatch);
}

TEST_CASE("malXml decodes entities outside CDATA and leaves CDATA alone") {
	auto titles = mal_xml::Parse(
	    "<anime>\n"
	    "  <series_title>  Tom &amp; Jerry &lt;3 &quot;x&quot; &#039;y&apos; "
	    "&amp;lt;  </series_title>\n"
	    "</anime>\n"
	    "<anime><series_title><![CDATA[ a &amp; b ]]></series_title></anime>\n");
	REQUIRE(titles.size() == 2);
	CHECK(titles[0].title == "Tom & Jerry <3 \"x\" 'y' &lt;");
	CHECK(titles[1].title == " a &amp; b ");
}

TEST_CASE("malXml keeps bad numbers and dates out of the catalogue") {
	auto titles = mal_xml::Parse(
	    "<anime><series_title>Odd</series_title>"
	    "<series_animedb_id>abc</series_animedb_id>"
	    "<series_episodes>-3</series_episodes>"
	    "<my_watched_episodes>99999999999999999999</my_watched_episodes>"
	    "<my_times_watched>1.5</my_times_watched>"
	    "<my_score>42</my_score>"
	    "<my_start_date>2023-02-30</my_start_date>"
	    "<my_finish_date>soon</my_finish_date></anime>");
	REQUIRE(titles.size() == 1);
	CHECK(titles[0].mal_id == 0);
	CHECK(titles[0].total_units == 0);
	CHECK(titles[0].progress == 0);
	CHECK(titles[0].rewatched == 0);
	CHECK(titles[0].score == 10);
	CHECK_FALSE(titles[0].start_date.has_value());
	CHECK_FALSE(titles[0].finish_date.has_value());
}

TEST_CASE("malXml skips untitled and unterminated blocks") {
	CHECK(mal_xml::Parse("").empty());
	CHECK(mal_xml::Parse("not xml at all").empty());
	CHECK(mal_xml::Parse("<anime><series_animedb_id>1</series_animedb_id></anime>").empty());
	CHECK(mal_xml::Parse("<anime><series_title>Cut off</series_title>").empty());
}

TEST_CASE("malXml writes one media type and leaves out titles without an id") {
	using catalist::test::MakeEntry;
	std::vector<catalist::Entry> entries = {
	    MakeEntry()
	        .MalId(52991)
	        .Title("Sousou no Frieren")
	        .TotalUnits(28)
	        .Progress(28)
	        .Rewatched(1)
	        .Score(10)
	        .Status(WatchStatus::kCompleted)
	        .StartDate("2023-09-29")
	        .FinishDate("2024-03-22"),
	    MakeEntry().Title("Home video"),
	    MakeEntry().MalId(2).Title("Berserk").Type(MediaType::kManga),
	};

	auto anime = mal_xml::Write(entries, MediaType::kAnime);
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
	      "\t\t<my_rewatching>0</my_rewatching>\n"
	      "\t\t<my_rewatch_value></my_rewatch_value>\n"
	      "\t\t<my_start_date>2023-09-29</my_start_date>\n"
	      "\t\t<my_finish_date>2024-03-22</my_finish_date>\n"
	      "\t\t<my_score>10</my_score>\n"
	      "\t\t<my_status>Completed</my_status>\n"
	      "\t\t<update_on_import>1</update_on_import>\n"
	      "\t</anime>\n"
	      "</myanimelist>\n");

	auto manga = mal_xml::Write(entries, MediaType::kManga);
	CHECK(manga.written == 1);
	CHECK(manga.skipped == 0);
	CHECK(manga.xml.find("<user_export_type>2</user_export_type>") != std::string::npos);
	CHECK(manga.xml.find("<manga_mangadb_id>2</manga_mangadb_id>") != std::string::npos);
	CHECK(manga.xml.find("<my_status>Reading</my_status>") != std::string::npos);
	CHECK(manga.xml.find("Frieren") == std::string::npos);
}

TEST_CASE("malXml reads back exactly what it wrote") {
	using catalist::test::MakeEntry;
	std::vector<catalist::Entry> entries = {
	    MakeEntry()
	        .MalId(1)
	        .Title("A <b> & ]]> \"title\"")
	        .TotalUnits(12)
	        .Progress(7)
	        .Rewatched(3)
	        .Score(8)
	        .Status(WatchStatus::kOnHold)
	        .StartDate("2020-01-02"),
	    MakeEntry().MalId(9).Title("Planned").Status(WatchStatus::kPlanToWatch),
	    MakeEntry().MalId(5).Title("Loved").Status(WatchStatus::kCompleted).RewatchValue(
	        catalist::RewatchValue::kVeryHigh),
	    MakeEntry().MalId(3).Title("Again").Progress(4).Status(WatchStatus::kRewatching),
	    MakeEntry()
	        .MalId(2)
	        .Title("Berserk")
	        .Type(MediaType::kManga)
	        .TotalUnits(380)
	        .Progress(364)
	        .TotalVolumes(41)
	        .Volumes(40)
	        .Rewatched(2)
	        .Score(10)
	        .Status(WatchStatus::kDropped),
	    MakeEntry()
	        .MalId(4)
	        .Type(MediaType::kManga)
	        .Title("Read again")
	        .Status(WatchStatus::kRewatching),
	};

	auto read_back = mal_xml::Parse(mal_xml::Write(entries, MediaType::kAnime).xml +
	                                mal_xml::Write(entries, MediaType::kManga).xml);
	REQUIRE(read_back.size() == entries.size());
	for (size_t i = 0; i < entries.size(); ++i) {
		CAPTURE(i);
		CHECK(read_back[i].type == entries[i].type);
		CHECK(read_back[i].mal_id == entries[i].mal_id);
		CHECK(read_back[i].title == entries[i].title);
		CHECK(read_back[i].status == entries[i].status);
		CHECK(read_back[i].score == entries[i].score);
		CHECK(read_back[i].progress == entries[i].progress);
		CHECK(read_back[i].total_units == entries[i].total_units);
		CHECK(read_back[i].volumes == entries[i].volumes);
		CHECK(read_back[i].total_volumes == entries[i].total_volumes);
		CHECK(read_back[i].rewatched == entries[i].rewatched);
		CHECK(read_back[i].rewatch_value == entries[i].rewatch_value);
		CHECK(read_back[i].start_date == entries[i].start_date);
		CHECK(read_back[i].finish_date == entries[i].finish_date);
	}
}

TEST_CASE("malXml reads the rewatch flag and value as enums") {
	auto titles = mal_xml::Parse(
	    "<anime><series_animedb_id>1</series_animedb_id><series_title>A</series_title>"
	    "<my_status>Completed</my_status><my_rewatching>1</my_rewatching>"
	    "<my_rewatch_value>2</my_rewatch_value></anime>"
	    "<anime><series_animedb_id>2</series_animedb_id><series_title>B</series_title>"
	    "<my_status>Completed</my_status><my_rewatching>0</my_rewatching>"
	    "<my_rewatch_value>9</my_rewatch_value></anime>"
	    "<manga><manga_mangadb_id>3</manga_mangadb_id><manga_title>C</manga_title>"
	    "<my_status>Completed</my_status><my_rereading>yes</my_rereading>"
	    "<my_reread_value>5</my_reread_value></manga>"
	    "<manga><manga_mangadb_id>4</manga_mangadb_id><manga_title>D</manga_title>"
	    "<my_status>Reading</my_status><my_rereading>NO</my_rereading>"
	    "<my_reread_value></my_reread_value></manga>");
	REQUIRE(titles.size() == 4);
	CHECK(titles[0].status == WatchStatus::kRewatching);
	CHECK(titles[0].rewatch_value == catalist::RewatchValue::kLow);
	CHECK(titles[1].status == WatchStatus::kCompleted);
	CHECK(titles[1].rewatch_value == catalist::RewatchValue::kNone);
	CHECK(titles[2].status == WatchStatus::kRewatching);
	CHECK(titles[2].rewatch_value == catalist::RewatchValue::kVeryHigh);
	CHECK(titles[3].status == WatchStatus::kWatching);
	CHECK(titles[3].rewatch_value == catalist::RewatchValue::kNone);
}

TEST_CASE("malXml keeps a finish date only on a completed or rewatched title") {
	auto block = [](const std::string& status, int id) {
		return std::format(
		    "<anime><series_animedb_id>{}</series_animedb_id><series_title>T{}</series_title>"
		    "<my_status>{}</my_status><my_finish_date>2021-12-31</my_finish_date></anime>",
		    id, id, status);
	};
	auto read = mal_xml::Parse(block("Dropped", 1) + block("On-Hold", 2) + block("Completed", 3));
	REQUIRE(read.size() == 3);
	CHECK_FALSE(read[0].finish_date.has_value());
	CHECK_FALSE(read[1].finish_date.has_value());
	CHECK(read[2].finish_date.has_value());
}
