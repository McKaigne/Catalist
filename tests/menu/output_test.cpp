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
	std::vector<output::TableColumn> columns = {{}, {.flexible = true}, {.align_right = true}, {}};
	CHECK(output::Rows(columns,
	                   {{{"1"}, {"Odd Taxi"}, {"8"}, {"+8"}}, {{"12"}, {"Berserk"}, {"-"}, {""}}},
	                   80, false) ==
	      "1   Odd Taxi  8  +8\n"
	      "12  Berserk   -\n");
}

TEST_CASE("output counts a CJK glyph as two columns") {
	std::vector<output::TableColumn> columns = {{.flexible = true}, {.align_right = true}};
	CHECK(
	    output::Rows(columns, {{{"葬送のフリーレン"}, {"10"}}, {{"Odd Taxi"}, {"8"}}}, 80, false) ==
	    "葬送のフリーレン  10\n"
	    "Odd Taxi           8\n");
}

TEST_CASE("output cuts the flexible column to fit the terminal") {
	std::vector<output::TableColumn> columns = {{}, {.flexible = true}, {.align_right = true}};
	std::vector<std::vector<output::Styled>> rows = {
	    {{"1"}, {"Sousou no Frieren: Beyond Journey's End"}, {"10"}}};
	CHECK(output::Rows(columns, rows, 24, false) == "1  Sousou no Frie...  10\n");
}

TEST_CASE("output groups rows under headings, leaving out empty sections") {
	std::vector<output::TableColumn> columns = {{.flexible = true}, {}};
	CHECK(output::Sections(columns,
	                       {{"ANIME", {{{"Odd Taxi"}, {"5/13"}}}},
	                        {"MANGA", {}},
	                        {"OTHER", {{{"Berserk"}, {"1/?"}}}}},
	                       80, false) == "ANIME\nOdd Taxi  5/13\n\nOTHER\nBerserk   1/?\n");
}

TEST_CASE("a score is five stars: full, half and empty") {
	CHECK(output::Stars(10) == "*****");
	CHECK(output::Stars(7) == "***+-");
	CHECK(output::Stars(1) == "+----");
	CHECK(output::Stars(8.5) == "****+");
	CHECK(output::Stars(8.4) == "****-");
	CHECK(output::Stars(0) == "-");
}

TEST_CASE("output draws bars and rules in ASCII") {
	CHECK(output::Bar(5, 10, 10, false) == "[#####-----]");
	CHECK(output::Bar(3, 0, 10, false).empty());
	CHECK(output::Rule(4, false) == "----");
}
