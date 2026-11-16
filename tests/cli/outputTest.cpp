#include <doctest/doctest.h>

#include "cli/output.hpp"
#include "support/testSupport.hpp"

namespace output = catalist::output;

TEST_CASE("output writes one line of tab-separated fields") {
	CHECK(output::tsvLine(std::vector<std::string>{"a:1", "anime", "Odd Taxi"}) ==
	      "a:1\tanime\tOdd Taxi\n");
	CHECK(output::tsvLine(std::vector<std::string>{"only"}) == "only\n");
	CHECK(output::tsvLine(std::vector<std::string>{}) == "\n");
}

TEST_CASE("output keeps a tab or a line break out of a field") {
	CHECK(output::tsvLine(std::vector<std::string>{"a\tb", "c\nd", "e\r\nf"}) ==
	      "a b\tc d\te  f\n");
	// Nothing else is changed: no quotes, no escapes.
	CHECK(output::tsvLine(std::vector<std::string>{"say \"hi\"", "back\\slash", "葬送"}) ==
	      "say \"hi\"\tback\\slash\t葬送\n");
}

TEST_CASE("output writes one line of comma-separated fields, quoted where it must be") {
	CHECK(output::csvLine(std::vector<std::string>{"a:1", "anime", "Odd Taxi"}) ==
	      "a:1,anime,Odd Taxi\n");
	CHECK(output::csvLine(std::vector<std::string>{"a, b", "say \"hi\"", "c\nd", "葬送"}) ==
	      "\"a, b\",\"say \"\"hi\"\"\",\"c\nd\",葬送\n");
	CHECK(output::csvLine(std::vector<std::string>{"", "x", ""}) == ",x,\n");
	CHECK(output::csvLine(std::vector<std::string>{}) == "\n");
}

TEST_CASE("output leaves a CSV field empty where JSON has null") {
	CHECK(output::csvLine(std::vector<output::cell>{
	          output::text("Odd Taxi"), output::number(5), output::known(0), output::known(13),
	          output::date(std::nullopt), output::maybe("")}) == "Odd Taxi,5,,13,,\n");
}

TEST_CASE("output gives every kind of value a text for a line") {
	CHECK(output::text("Odd Taxi").text == "Odd Taxi");
	CHECK(output::number(13).text == "13");
	CHECK(output::known(13).text == "13");
	CHECK(output::known(0).text == "0");
	CHECK(output::date(catalist::test::requireDate("2026-10-20")).text == "2026-10-20");
	CHECK(output::date(std::nullopt).text == "-");
	CHECK(output::maybe("https://anilist.co/anime/1").text == "https://anilist.co/anime/1");
	CHECK(output::maybe("").text == "-");
	CHECK(output::tsvLine({output::text("a:1"), output::known(0), output::date(std::nullopt)}) ==
	      "a:1\t0\t-\n");
}

TEST_CASE("output escapes a JSON string") {
	CHECK(output::jsonString("Odd Taxi") == "\"Odd Taxi\"");
	CHECK(output::jsonString("") == "\"\"");
	CHECK(output::jsonString("say \"hi\"") == "\"say \\\"hi\\\"\"");
	CHECK(output::jsonString("back\\slash") == "\"back\\\\slash\"");
	CHECK(output::jsonString("a\tb\nc\rd") == "\"a\\tb\\nc\\rd\"");
	CHECK(output::jsonString(std::string("bell\x07nul\x00!", 10)) == "\"bell\\u0007nul\\u0000!\"");
	CHECK(output::jsonString("\x1f") == "\"\\u001f\"");
	// UTF-8 goes through as it is.
	CHECK(output::jsonString("葬送のフリーレン") == "\"葬送のフリーレン\"");
	CHECK(output::jsonString("a/b") == "\"a/b\"");
}

TEST_CASE("output writes one JSON object a line, numbers and nulls unquoted") {
	CHECK(output::jsonLine(
	          {"ref", "title", "score", "total", "started", "finished", "url"},
	          {output::text("a:1"), output::text("Odd \"Taxi\""), output::number(8),
	           output::known(0), output::date(std::nullopt),
	           output::date(catalist::test::requireDate("2026-10-20")), output::maybe("")}) ==
	      "{\"ref\":\"a:1\",\"title\":\"Odd \\\"Taxi\\\"\",\"score\":8,\"total\":null,"
	      "\"started\":null,\"finished\":\"2026-10-20\",\"url\":null}\n");
	CHECK(output::jsonLine({"total"}, {output::known(13)}) == "{\"total\":13}\n");
	// A title that is all digits is still a string.
	CHECK(output::jsonLine({"title"}, {output::text("86")}) == "{\"title\":\"86\"}\n");
	CHECK(output::jsonLine({}, {}) == "{}\n");
}

TEST_CASE("output paints only when colours are on") {
	CHECK(output::paint("new", output::kGreen, true) == "\x1b[32mnew\x1b[0m");
	CHECK(output::paint("new", output::kGreen, false) == "new");
	CHECK(output::paint("plain", nullptr, true) == "plain");
}

TEST_CASE("output draws a table as wide as what it holds") {
	std::vector<output::tableColumn> columns = {
	    {"REF"}, {"TITLE", false, true}, {"SCORE", true}, {"NEW"}};
	CHECK(output::table(
	          columns,
	          {{{"a:1"}, {"Odd Taxi"}, {"8"}, {"+8"}}, {{"m:12"}, {"Berserk"}, {"-"}, {""}}}, 80,
	          false) ==
	      "REF   TITLE     SCORE  NEW\n"
	      "a:1   Odd Taxi      8  +8\n"
	      "m:12  Berserk       -\n");
	// No rows: the header alone.
	CHECK(output::table(columns, {}, 80, false) == "REF  TITLE  SCORE  NEW\n");
}

TEST_CASE("output counts a CJK glyph as two columns") {
	std::vector<output::tableColumn> columns = {{"TITLE", false, true}, {"SCORE", true}};
	CHECK(output::table(columns, {{{"葬送のフリーレン"}, {"10"}}, {{"Odd Taxi"}, {"8"}}}, 80,
	                    false) ==
	      "TITLE             SCORE\n"
	      "葬送のフリーレン     10\n"
	      "Odd Taxi              8\n");
}

TEST_CASE("output cuts the flexible column to fit the terminal") {
	std::vector<output::tableColumn> columns = {{"REF"}, {"TITLE", false, true}, {"SCORE", true}};
	std::vector<std::vector<output::styled>> rows = {
	    {{"a:1"}, {"Sousou no Frieren: Beyond Journey's End"}, {"10"}},
	    {{"a:2"}, {"葬送のフリーレン 葬送のフリーレン"}, {"9"}}};
	CHECK(output::table(columns, rows, 30, false) ==
	      "REF  TITLE               SCORE\n"
	      "a:1  Sousou no Frieren…     10\n"
	      "a:2  葬送のフリーレン …      9\n");
	// It is never cut to nothing, however narrow the terminal.
	CHECK(output::table(columns, rows, 5, false).find("a:1  Sousou no…     10\n") !=
	      std::string::npos);
}

TEST_CASE("output paints a table's cells, the header dim") {
	std::vector<output::tableColumn> columns = {{"REF"}, {"NEW"}};
	CHECK(output::table(columns, {{{"a:1", output::kCyan}, {"+8", output::kGreen}}}, 80, true) ==
	      "\x1b[2mREF\x1b[0m  \x1b[2mNEW\x1b[0m\n"
	      "\x1b[36ma:1\x1b[0m  \x1b[32m+8\x1b[0m\n");
}
