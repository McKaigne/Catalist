#include "cli/output.hpp"

#include <doctest/doctest.h>

#include "support/test_support.hpp"

namespace output = catalist::output;

TEST_CASE("output writes one line of tab-separated fields") {
	CHECK(output::TsvLine(std::vector<std::string>{"a:1", "anime", "Odd Taxi"}) ==
	      "a:1\tanime\tOdd Taxi\n");
	CHECK(output::TsvLine(std::vector<std::string>{"only"}) == "only\n");
	CHECK(output::TsvLine(std::vector<std::string>{}) == "\n");
}

TEST_CASE("output keeps a tab or a line break out of a field") {
	CHECK(output::TsvLine(std::vector<std::string>{"a\tb", "c\nd", "e\r\nf"}) ==
	      "a b\tc d\te  f\n");
	// Nothing else is changed: no quotes, no escapes.
	CHECK(output::TsvLine(std::vector<std::string>{"say \"hi\"", "back\\slash", "葬送"}) ==
	      "say \"hi\"\tback\\slash\t葬送\n");
}

TEST_CASE("output writes one line of comma-separated fields, quoted where it must be") {
	CHECK(output::CsvLine(std::vector<std::string>{"a:1", "anime", "Odd Taxi"}) ==
	      "a:1,anime,Odd Taxi\n");
	CHECK(output::CsvLine(std::vector<std::string>{"a, b", "say \"hi\"", "c\nd", "葬送"}) ==
	      "\"a, b\",\"say \"\"hi\"\"\",\"c\nd\",葬送\n");
	CHECK(output::CsvLine(std::vector<std::string>{"", "x", ""}) == ",x,\n");
	CHECK(output::CsvLine(std::vector<std::string>{}) == "\n");
}

TEST_CASE("output leaves a CSV field empty where JSON has null") {
	CHECK(output::CsvLine(std::vector<output::Cell>{
	          output::Text("Odd Taxi"), output::Number(5), output::Known(0), output::Known(13),
	          output::Date(std::nullopt), output::Maybe("")}) == "Odd Taxi,5,,13,,\n");
}

TEST_CASE("output gives every kind of value a text for a line") {
	CHECK(output::Text("Odd Taxi").text == "Odd Taxi");
	CHECK(output::Number(13).text == "13");
	CHECK(output::Known(13).text == "13");
	CHECK(output::Known(0).text == "0");
	CHECK(output::Date(catalist::test::RequireDate("2026-10-20")).text == "2026-10-20");
	CHECK(output::Date(std::nullopt).text == "-");
	CHECK(output::Maybe("https://anilist.co/anime/1").text == "https://anilist.co/anime/1");
	CHECK(output::Maybe("").text == "-");
	CHECK(output::TsvLine({output::Text("a:1"), output::Known(0), output::Date(std::nullopt)}) ==
	      "a:1\t0\t-\n");
}

TEST_CASE("output escapes a JSON string") {
	CHECK(output::JsonString("Odd Taxi") == "\"Odd Taxi\"");
	CHECK(output::JsonString("") == "\"\"");
	CHECK(output::JsonString("say \"hi\"") == "\"say \\\"hi\\\"\"");
	CHECK(output::JsonString("back\\slash") == "\"back\\\\slash\"");
	CHECK(output::JsonString("a\tb\nc\rd") == "\"a\\tb\\nc\\rd\"");
	CHECK(output::JsonString(std::string("bell\x07nul\x00!", 10)) == "\"bell\\u0007nul\\u0000!\"");
	CHECK(output::JsonString("\x1f") == "\"\\u001f\"");
	// UTF-8 goes through as it is.
	CHECK(output::JsonString("葬送のフリーレン") == "\"葬送のフリーレン\"");
	CHECK(output::JsonString("a/b") == "\"a/b\"");
}

TEST_CASE("output writes one JSON object a line, numbers and nulls unquoted") {
	CHECK(output::JsonLine(
	          {"ref", "title", "score", "total", "started", "finished", "url"},
	          {output::Text("a:1"), output::Text("Odd \"Taxi\""), output::Number(8),
	           output::Known(0), output::Date(std::nullopt),
	           output::Date(catalist::test::RequireDate("2026-10-20")), output::Maybe("")}) ==
	      "{\"ref\":\"a:1\",\"title\":\"Odd \\\"Taxi\\\"\",\"score\":8,\"total\":null,"
	      "\"started\":null,\"finished\":\"2026-10-20\",\"url\":null}\n");
	CHECK(output::JsonLine({"total"}, {output::Known(13)}) == "{\"total\":13}\n");
	// A title that is all digits is still a string.
	CHECK(output::JsonLine({"title"}, {output::Text("86")}) == "{\"title\":\"86\"}\n");
	CHECK(output::JsonLine({}, {}) == "{}\n");
}

TEST_CASE("output paints only when colours are on") {
	CHECK(output::Paint("new", output::kGreen, true) == "\x1b[32mnew\x1b[0m");
	CHECK(output::Paint("new", output::kGreen, false) == "new");
	CHECK(output::Paint("plain", nullptr, true) == "plain");
}

TEST_CASE("output draws a table as wide as what it holds") {
	std::vector<output::TableColumn> columns = {
	    {"REF"}, {"TITLE", false, true}, {"SCORE", true}, {"NEW"}};
	CHECK(output::Table(
	          columns,
	          {{{"a:1"}, {"Odd Taxi"}, {"8"}, {"+8"}}, {{"m:12"}, {"Berserk"}, {"-"}, {""}}}, 80,
	          false) ==
	      "REF   TITLE     SCORE  NEW\n"
	      "a:1   Odd Taxi      8  +8\n"
	      "m:12  Berserk       -\n");
	// No rows: the header alone.
	CHECK(output::Table(columns, {}, 80, false) == "REF  TITLE  SCORE  NEW\n");
}

TEST_CASE("output counts a CJK glyph as two columns") {
	std::vector<output::TableColumn> columns = {{"TITLE", false, true}, {"SCORE", true}};
	CHECK(output::Table(columns, {{{"葬送のフリーレン"}, {"10"}}, {{"Odd Taxi"}, {"8"}}}, 80,
	                    false) ==
	      "TITLE             SCORE\n"
	      "葬送のフリーレン     10\n"
	      "Odd Taxi              8\n");
}

TEST_CASE("output cuts the flexible column to fit the terminal") {
	std::vector<output::TableColumn> columns = {{"REF"}, {"TITLE", false, true}, {"SCORE", true}};
	std::vector<std::vector<output::Styled>> rows = {
	    {{"a:1"}, {"Sousou no Frieren: Beyond Journey's End"}, {"10"}},
	    {{"a:2"}, {"葬送のフリーレン 葬送のフリーレン"}, {"9"}}};
	CHECK(output::Table(columns, rows, 30, false) ==
	      "REF  TITLE               SCORE\n"
	      "a:1  Sousou no Frieren…     10\n"
	      "a:2  葬送のフリーレン …      9\n");
	// It is never cut to nothing, however narrow the terminal.
	CHECK(output::Table(columns, rows, 5, false).find("a:1  Sousou no…     10\n") !=
	      std::string::npos);
}

TEST_CASE("output paints a table's cells, the header dim") {
	std::vector<output::TableColumn> columns = {{"REF"}, {"NEW"}};
	CHECK(output::Table(columns, {{{"a:1", output::kCyan}, {"+8", output::kGreen}}}, 80, true) ==
	      "\x1b[2mREF\x1b[0m  \x1b[2mNEW\x1b[0m\n"
	      "\x1b[36ma:1\x1b[0m  \x1b[32m+8\x1b[0m\n");
}

TEST_CASE("a score is five Nerd Font stars: full, half filled and empty") {
	const std::string f = "\U000F04CE";
	const std::string h = "\U000F04D0";
	const std::string e = "\U000F04D2";
	CHECK(catalist::output::Stars(10) == f + f + f + f + f);
	CHECK(catalist::output::Stars(7) == f + f + f + h + e);
	CHECK(catalist::output::Stars(1) == h + e + e + e + e);
	CHECK(catalist::output::Stars(8.5) == f + f + f + f + h);
	CHECK(catalist::output::Stars(8.4) == f + f + f + f + e);
	CHECK(catalist::output::Stars(0) == "-");
}
