#include "menu/output.hpp"

#include <doctest/doctest.h>

#include "support/test_support.hpp"

namespace output = catalist::output;

TEST_CASE("output writes a dash for a date that is not there") {
	CHECK(output::DateText(catalist::test::RequireDate("2026-10-20")) == "2026-10-20");
	CHECK(output::DateText(std::nullopt) == "-");
}

TEST_CASE("output paints only when colours are on") {
	CHECK(output::Paint("new", output::kGreen, true) == "\x1b[32mnew\x1b[0m");
	CHECK(output::Paint("new", output::kGreen, false) == "new");
	CHECK(output::Paint("plain", nullptr, true) == "plain");
}

TEST_CASE("output aligns rows to what they hold") {
	std::vector<output::TableColumn> columns = {{""}, {"", false, true}, {"", true}, {""}};
	CHECK(output::Rows(columns,
	                   {{{"1"}, {"Odd Taxi"}, {"8"}, {"+8"}}, {{"12"}, {"Berserk"}, {"-"}, {""}}},
	                   80, false) ==
	      "1   Odd Taxi  8  +8\n"
	      "12  Berserk   -\n");
}

TEST_CASE("output counts a CJK glyph as two columns") {
	std::vector<output::TableColumn> columns = {{"", false, true}, {"", true}};
	CHECK(output::Rows(columns, {{{"葬送のフリーレン"}, {"10"}}, {{"Odd Taxi"}, {"8"}}}, 80, false) ==
	      "葬送のフリーレン  10\n"
	      "Odd Taxi           8\n");
}

TEST_CASE("output cuts the flexible column to fit the terminal") {
	std::vector<output::TableColumn> columns = {{""}, {"", false, true}, {"", true}};
	std::vector<std::vector<output::Styled>> rows = {
	    {{"1"}, {"Sousou no Frieren: Beyond Journey's End"}, {"10"}}};
	CHECK(output::Rows(columns, rows, 24, false) == "1  Sousou no Friere…  10\n");
}

TEST_CASE("output groups rows under headings, leaving out empty sections") {
	std::vector<output::TableColumn> columns = {{"", false, true}, {""}};
	CHECK(output::Sections(columns,
	                       {{"ANIME", {{{"Odd Taxi"}, {"5/13"}}}},
	                        {"MANGA", {}},
	                        {"OTHER", {{{"Berserk"}, {"1/?"}}}}},
	                       80, false) ==
	      "ANIME\nOdd Taxi  5/13\n\nOTHER\nBerserk   1/?\n");
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
