#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "core/version.hpp"
#include "support/cli_fixture.hpp"

using catalist::MediaType;
using catalist::WatchStatus;
using catalist::test::CliFixture;
using catalist::test::Has;
using catalist::test::kAnimeXml;

TEST_CASE_FIXTURE(CliFixture, "cli --version prints the version") {
	for (const char* flag : {"--version", "-V", "-v"}) {
		CHECK(Run({flag}) == 0);
		CHECK(out == std::string("Catalist v") + catalist::kVersionString + "\n");
		CHECK(err.empty());
	}
}

TEST_CASE_FIXTURE(CliFixture, "cli help lists the commands, and each command has its own") {
	for (const char* flag : {"--help", "-h", "help"}) {
		CHECK(Run({flag}) == 0);
		CHECK(Has(out, "Usage: catalist [OPTION...] COMMAND [ARG...]\n"));
		CHECK(Has(out,
		          "\nOptions, before the command:\n"
		          "  -V, --version     print the version\n"
		          "  -h, --help        show the commands\n"));
		CHECK(Has(out, "  ls          List your titles\n"));
		CHECK(Has(out, "  +           Count one more episode or chapter\n"));
		CHECK(Has(out, "  undo        Take back what the last command changed\n"));
		CHECK(Has(out, "  help        Show help for a command\n"));
		CHECK(Has(out, "  add         Add a title found on AniList\n"));
		CHECK(Has(out, "  catalist show a:12\n"));
		CHECK(Has(out, "kept in\ndata.\n"));
		CHECK(err.empty());
	}
	CHECK(Run({"help", "ls"}) == 0);
	CHECK(out.find("List your titles.\n\n"
	               "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [QUERY...]\n") == 0);
	CHECK(Has(out, "ref type title status score progress total volumes rewatched\n"));
	CHECK(Has(out, "\nOptions:\n  -a                list anime only\n"));
	CHECK(Has(
	    out,
	    "  -s STATUS         only watching, completed, on-hold, dropped, planned, rewatching, or a,b\n"));
	CHECK(Has(out, "      --sort=KEY    updated, score, progress, date, status or title\n"));
	CHECK(out.rfind("  -h, --help        show this help\n") + 35 == out.size());
	CHECK(err.empty());
	std::string from_help = out;
	for (const char* flag : {"-h", "--help"}) {
		CHECK(Run({"ls", flag}) == 0);
		CHECK(out == from_help);
	}
	// Asked for among other options, help is all that happens.
	CHECK(Run({"ls", "-qh", "--sort=title"}) == 0);
	CHECK(out == from_help);
	CHECK(Run({"help", "show"}) == 0);
	CHECK(Has(out, "Usage: catalist show [-am] [--all] REF...\n"));
	CHECK(Run({"help", "+"}) == 0);
	CHECK(Has(out, "Usage: catalist + [-amq] [--all] [-n N] [REF...]\nAlso:  catalist inc\n"));
	CHECK(Run({"-", "--help"}) == 0);
	CHECK(Has(out, "Usage: catalist - [-amq] [--all] [-n N] [REF...]\nAlso:  catalist dec\n"));
	CHECK(Run({"help", "set"}) == 0);
	CHECK(Has(out, "Usage: catalist set [-amq] [--all] REF FIELD=VALUE...\n"));
	CHECK(Run({"help", "done"}) == 0);
	CHECK(Has(out, "Usage: catalist done [-amq] [--all] [--score=N] [REF...]\n"));
	CHECK(Run({"help", "hold"}) == 0);
	CHECK(Has(out, "Usage: catalist hold [-amq] [--all] [REF...]\n"));
	CHECK(Run({"help", "undo"}) == 0);
	CHECK(Has(out, "Usage: catalist undo [-fq] [-n N]\n"));
	CHECK(Run({"help", "add"}) == 0);
	CHECK(Has(out,
	          "Usage: catalist add [-amq] [--pick=N] [--status=STATUS] "
	          "[--score=N] [--progress=N] [--no-lookup] TITLE...\n"));
	// The helps line up past the longest option.
	CHECK(Has(
	    out,
	    "      --status=STATUS  watching, completed, on-hold, dropped, planned or rewatching\n"));
	CHECK(Has(out, "      --no-lookup      add TITLE as written, without asking a site\n"));
	CHECK(Has(out, "  -q                   print nothing\n"));
	CHECK(Run({"help", "rm"}) == 0);
	CHECK(Has(out, "Usage: catalist rm [-amfq] [--all] REF...\n"));
	CHECK(Run({"help", "search"}) == 0);
	CHECK(Has(out, "Usage: catalist search [-am] QUERY...\n"));
	CHECK(Has(out, "  pick type title year total score anilist_id mal_id ref\n"));
	CHECK(Run({"help", "stats"}) == 0);
	CHECK(Has(out, "Usage: catalist stats [-am]\n"));
	CHECK(Run({"help", "import"}) == 0);
	CHECK(Has(out, "Usage: catalist import [-q] FILE...\n"));
	CHECK(Run({"help", "export"}) == 0);
	CHECK(Has(out, "Usage: catalist export anime|manga\n"));

	CHECK(Run({"help", "frobnicate"}) == 2);
	CHECK(err ==
	      "error: no command 'frobnicate'\n"
	      "Usage: catalist help [COMMAND]\n"
	      "Run 'catalist help help' for more.\n");
	CHECK(out.empty());
	CHECK(Run({"help", "shwo"}) == 2);
	CHECK(err.find("error: no command 'shwo'\n") == 0);
	CHECK(Run({"help", "ls", "show"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli help and version work with a broken catalogue") {
	Put("data/catalist.db", "this is not a catalogue\n");
	CHECK(Run({"--version"}) == 0);
	CHECK(Run({"--help"}) == 0);
	CHECK(Run({"set", "--help"}) == 0);
}

TEST_CASE_FIXTURE(CliFixture,
                  "cli refuses a command or option it does not know, with exit code 2") {
	CHECK(Run({"lst"}) == 2);
	CHECK(err ==
	      "error: unknown command 'lst'\n"
	      "Usage: catalist [OPTION...] COMMAND [ARG...]\n"
	      "Run 'catalist help' to see the commands.\n");
	CHECK(out.empty());
	CHECK(Run({"frobnicate"}) == 2);
	CHECK(err.find("error: unknown command 'frobnicate'\n") == 0);
	CHECK(Run({"--frobnicate"}) == 2);
	CHECK(err.find("error: unknown option '--frobnicate'\nUsage: catalist [OPTION...] COMMAND") ==
	      0);
	// The names the earlier command line had are gone.
	for (const char* gone :
	     {"list", "info", "version", "next", "edit", "remove", "delete", "login", "logout",
	      "accounts", "sync", "link", "unlink", "track", "refresh", "provider", "completion"})
		CHECK(Run({gone}) == 2);
	CHECK(Run({"udno"}) == 2);
	CHECK(err.find("error: unknown command 'udno'\n") == 0);

	CHECK(Run({"ls", "--json"}) == 2);
	CHECK(err ==
	      "error: unknown option '--json'\n"
	      "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [QUERY...]\n"
	      "Run 'catalist help ls' for more.\n");
	CHECK(Run({"ls", "-ax"}) == 2);
	CHECK(err.find("error: unknown option '-x'\n") == 0);
	CHECK(Run({"ls", "--sort"}) == 2);
	CHECK(err.find("error: option '--sort' needs a value (KEY)\n") == 0);
	CHECK(Run({"ls", "-s"}) == 2);
	CHECK(err.find("error: option '-s' needs a value (STATUS)\n") == 0);
	CHECK(Run({"show", "--all=yes", "taxi"}) == 2);
	CHECK(err.find("error: option '--all' takes no value\n") == 0);
	CHECK(Run({"show", "--al", "taxi"}) == 2);
	CHECK(err.find("error: unknown option '--al'\n") == 0);
	CHECK(out.empty());

	CHECK(Run({"add", "--stauts", "watching", "frieren"}) == 2);
	CHECK(err.find("error: unknown option '--stauts'\n"
	               "Usage: catalist add ") == 0);
	CHECK(Has(err, "Run 'catalist help add' for more.\n"));
	CHECK(Run({"add", "--pick"}) == 2);
	CHECK(err.find("error: option '--pick' needs a value (N)\n") == 0);
	CHECK(Run({"rm", "--yes", "taxi"}) == 2);
	CHECK(Run({"import"}) == 2);
	CHECK(err.find("error: give at least one file, or - for standard input\n") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli stops quietly when nothing reads its output any more") {
	Seed();
	out_closed = true;
	for (const auto& args :
	     {std::vector<std::string>{"ls"}, {"ls", "-q"}, {"show", "taxi,", "berserk"}, {"help"}}) {
		CHECK(Run(args) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
	CHECK(Run({"ls"}, "", 100) == 0);
	CHECK(Run({"show", "--all", "frieren"}, "", 100) == 0);
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli says which title it could not find, and shows none of them") {
	Seed();
	CHECK(Run({"show", "naruto"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches 'naruto'\n"
	      "See what is there with: catalist ls\n");
	CHECK(Run({"show", "-a", "berserk"}) == 1);
	CHECK(err.find("error: nothing in your anime list matches 'berserk'\n") == 0);
	CHECK(Run({"show", "a:9"}) == 1);
	CHECK(err.find("error: no title in your lists has the ref a:9\n") == 0);
	CHECK(Run({"show", "-m", "a:2"}) == 1);
	CHECK(err.find("error: no title in your manga list has the ref a:2\n") == 0);
	// One REF that fails stops them all.
	CHECK(Run({"show", "taxi,", "naruto"}) == 1);
	CHECK(out.empty());
	CHECK(Run({"show", "frieren", "--all"}) == 0);
	CHECK(Has(out, "\nm:2\t"));
}

TEST_CASE_FIXTURE(CliFixture, "cli suggests the closest title and changes nothing") {
	Seed();
	CHECK(Run({"+", "freiren"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches 'freiren'\n"
	      "Did you mean Sousou no Frieren?\n");
	CHECK(Run({"+", "taxo"}) == 1);
	CHECK(Has(err, "Did you mean Odd Taxi?\n"));
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 5);
	CHECK(Steps().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli takes the one in-progress match, only at a terminal") {
	Seed();
	REQUIRE(Run({"resume", "m:2"}) == 0);
	CHECK(Run({"+", "frieren"}, "unused\n") == 0);
	CHECK(SavedTitle(MediaType::kManga, "Sousou no Frieren").progress == 1);
	CHECK(Run({"+", "frieren"}) == 1);
	CHECK(Has(err, "matches 2 titles"));
	// rewatch prefers what it can start: the completed one.
	CHECK(Run({"rewatch", "frieren"}, "unused\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").status == WatchStatus::kRewatching);
}

TEST_CASE_FIXTURE(CliFixture, "cli offers titles when none is named, at a terminal only") {
	Seed();
	CHECK(Run({"+"}, "1\n") == 0);
	CHECK(Has(err, "   1  Berserk   manga  364/?\n   2  Odd Taxi  anime  5/13\n"));
	CHECK(Has(err, "Which one? [1-2, Enter to cancel]: "));
	CHECK(Run({"+"}) == 2);
	CHECK(Has(err, "error: name a title"));

	CHECK(Run({"rewatch"}, "1\n") == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Sousou no Frieren").status == WatchStatus::kRewatching);
	CHECK(Run({"resume"}, "1\n") == 1);
	CHECK(Has(err, "error: no title to choose from; name one\n"));
	CHECK(Run({"rm"}, "1\n") == 2);
	CHECK(Run({"show"}, "1\n") == 2);
	CHECK(Run({"+"}, "\n") == 1);
	CHECK(Has(err, "Cancelled.\n"));
}

TEST_CASE_FIXTURE(CliFixture, "cli offers only the 20 latest titles") {
	std::string xml = "<myanimelist>";
	for (int i = 1; i <= 25; ++i)
		xml += std::format(
		    "<anime><series_animedb_id>{0}</series_animedb_id><series_title>Show {0}"
		    "</series_title><my_status>Watching</my_status></anime>",
		    i);
	Put("many.xml", xml + "</myanimelist>");
	REQUIRE(Run({"import", "many.xml"}) == 0);
	CHECK(Run({"+"}, "\n") == 1);
	CHECK(Has(err, "Which one? [1-20, Enter to cancel]: "));
	CHECK(Has(err, "  and 5 more: name one to reach it\n"));
}

TEST_CASE_FIXTURE(CliFixture, "cli names several titles with commas") {
	Seed();
	CHECK(Run({"+", "odd", "taxi,", "berserk"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(SavedTitle(MediaType::kManga, "Berserk").progress == 365);
	CHECK(Run({"+", "taxi", "-n", "2"}) == 0);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 8);
	CHECK(Run({"set", "odd", "taxi,", "berserk", "score=3"}) == 0);
	CHECK(SavedTitle(MediaType::kManga, "Berserk").score == 3);
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").score == 3);
	CHECK(Run({"+", ","}) == 2);
	CHECK(Run({"+", "!!"}) == 2);
	CHECK(Has(err, "punctuation alone is not a title"));
}

TEST_CASE_FIXTURE(CliFixture, "cli asks which title was meant, or lists them into a pipe") {
	Seed();
	CHECK(Run({"show", "frieren"}) == 1);
	CHECK(err ==
	      "error: 'frieren' matches 2 titles:\n"
	      "  a:1  Sousou no Frieren\n"
	      "  m:2  Sousou no Frieren\n"
	      "Name one by its ref (-a and -m keep to one list), or pass --all for all of them.\n");
	CHECK(out.empty());
	CHECK(Run({"show", "-m", "frieren"}) == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK(Run({"show", "--all", "frieren"}) == 0);
	CHECK(out.find("a:1\t") == 0);
	CHECK(Has(out, "\nm:2\t"));

	CHECK(Run({"show", "frieren"}, "2\n") == 0);
	CHECK(err ==
	      "'frieren' matches 2 titles:\n"
	      "   1  Sousou no Frieren  anime  28/28\n"
	      "   2  Sousou no Frieren  manga  0/?\n"
	      "Which one? [1-2, Enter to cancel]: ");
	CHECK(out.find("m:2\t") == 0);
	for (const char* answer : {"\n", "7\n", "both\n"}) {
		CHECK(Run({"show", "frieren"}, answer) == 1);
		CHECK(Has(err, "]: Cancelled.\n"));
		CHECK(out.empty());
	}
}

TEST_CASE_FIXTURE(CliFixture, "cli says when a command changed nothing, and records no step") {
	Seed();
	// Frieren is complete: there is no more to count.
	CHECK(Run({"+", "a:1"}) == 0);
	CHECK(out.empty());
	CHECK(err ==
	      "Nothing changed.\n"
	      "Sousou no Frieren is at its total, 28 episodes. If there are more: catalist set TITLE "
	      "total=N\n");
	CHECK(Run({"set", "taxi", "score=8", "progress=5"}) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(Run({"resume", "taxi"}) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(Run({"done", "a:1"}, "", 100) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(out.empty());
	// -q prints nothing, not even that.
	CHECK(Run({"+", "-q", "a:1"}) == 0);
	CHECK(err.empty());
	CHECK(Steps().empty());
	// Only the titles that changed are printed.
	CHECK(Run({"+", "--all", "frieren"}) == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK_FALSE(Has(out, "a:1\t"));
}

TEST_CASE_FIXTURE(CliFixture, "cli stops quietly when nobody reads what a change printed") {
	Seed();
	out_closed = true;
	CHECK(Run({"+", "--all", "frieren,", "taxi"}) == 0);
	CHECK(err.empty());
	CHECK(SavedTitle(MediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(Run({"undo"}) == 0);
	CHECK(Run({"log"}) == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli refuses to work on a catalogue it cannot read") {
	Put("data/catalist.db", "this is not a catalogue\n");
	Put("lists/anime.xml", kAnimeXml);
	CHECK(Run({"import", "lists/anime.xml"}) == 1);
	CHECK(err.find("error: ") == 0);
	CHECK(Has(err, "data/catalist.db"));
	for (const char* command : {"stats", "ls"})
		CHECK(Run({command}) == 1);
	CHECK(Run({"export", "anime"}) == 1);
	CHECK(Run({"rm", "-f", "taxi"}) == 1);
	CHECK(Run({"add", "--no-lookup", "Home Video"}) == 1);
	CHECK(out.empty());
	CHECK(catalist::test::ReadText("data/catalist.db") == "this is not a catalogue\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli alone at a terminal lists what is being watched") {
	Seed();
	CHECK(Run({}, "", 100) == 0);
	CHECK(Has(out, "Odd Taxi"));
	CHECK(Has(out, "Berserk"));
	CHECK_FALSE(Has(out, "Frieren"));
	CHECK(err.empty());
	// The title changed last comes first.
	CHECK(out.find("Berserk") < out.find("Odd Taxi"));
	CHECK(Run({"+", "-q", "taxi"}) == 0);
	CHECK(Run({}, "", 100) == 0);
	CHECK(out.find("Odd Taxi") < out.find("Berserk"));
}

TEST_CASE_FIXTURE(CliFixture, "cli without a command says how to give one, with exit code 2") {
	CHECK(Run({}) == 2);
	CHECK(out.empty());
	CHECK(err ==
	      "error: no command given\nUsage: catalist [OPTION...] COMMAND [ARG...]\n"
	      "Run 'catalist help' to see the commands.\n");
	CHECK_FALSE(std::filesystem::exists("data"));
}
