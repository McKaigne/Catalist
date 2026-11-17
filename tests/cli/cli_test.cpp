#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/version.hpp"
#include "support/cli_fixture.hpp"

using catalist::MediaType;
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
		          "      --color=WHEN  colours: auto (at a terminal), always or never\n"
		          "  -t, --table       draw tables into a pipe as well ($COLUMNS wide)\n"));
		CHECK(Has(out, "  ls          List your titles\n"));
		CHECK(Has(out, "  +           Count one more episode or chapter\n"));
		CHECK(Has(out, "  undo        Take back what the last command changed\n"));
		CHECK(Has(out, "  help        Show help for a command\n"));
		CHECK(Has(out, "  add         Add a title found on AniList, MyAnimeList or Kitsu\n"));
		CHECK(Has(out, "  catalist show a:12\n"));
		CHECK(Has(out, "kept in\ndata.\n"));
		CHECK(err.empty());
	}
	CHECK(Run({"help", "ls"}) == 0);
	CHECK(out.find("List your titles.\n\n"
	               "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [--linked=SITE] "
	               "[--unlinked=SITE] [--json] [--header] [QUERY...]\n") == 0);
	CHECK(Has(out, "ref type title status score progress total volumes rewatched\n"));
	CHECK(Has(out, "\nOptions:\n  -a                   list anime only\n"));
	CHECK(Has(
	    out,
	    "  -s STATUS            only watching, completed, on-hold, dropped, planned, rewatching, or a,b\n"));
	CHECK(
	    Has(out, "      --sort=KEY       new, updated, score, progress, date, status or title\n"));
	CHECK(out.rfind("  -h, --help           show this help\n") + 38 == out.size());
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
	CHECK(Has(out, "Usage: catalist show [-am] [--all] [--json] [--header] REF...\n"));
	CHECK(Run({"help", "+"}) == 0);
	CHECK(Has(out, "Usage: catalist + [-amq] [--all] [-n N] REF...\nAlso:  catalist inc\n"));
	CHECK(Run({"-", "--help"}) == 0);
	CHECK(Has(out, "Usage: catalist - [-amq] [--all] [-n N] REF...\nAlso:  catalist dec\n"));
	CHECK(Run({"help", "set"}) == 0);
	CHECK(Has(out, "Usage: catalist set [-amq] [--all] REF FIELD=VALUE...\n"));
	CHECK(Run({"help", "done"}) == 0);
	CHECK(Has(out, "Usage: catalist done [-amq] [--all] [--score=N] REF...\n"));
	CHECK(Run({"help", "hold"}) == 0);
	CHECK(Has(out, "Usage: catalist hold [-amq] [--all] REF...\n"));
	CHECK(Run({"help", "undo"}) == 0);
	CHECK(Has(out, "Usage: catalist undo [-fq] [-n N]\n"));
	CHECK(Run({"help", "add"}) == 0);
	CHECK(Has(out,
	          "Usage: catalist add [-amq] [--from=SITE] [--pick=N] [--status=STATUS] "
	          "[--score=N] [--progress=N] [--no-lookup] TITLE...\n"));
	// The helps line up past the longest option.
	CHECK(Has(
	    out,
	    "      --status=STATUS  watching, completed, on-hold, dropped, planned or rewatching\n"));
	CHECK(Has(out, "      --no-lookup      add TITLE as written, without asking a site\n"));
	CHECK(Has(out, "  -q                   print nothing\n"));
	CHECK(Run({"help", "rm"}) == 0);
	CHECK(Has(out, "Usage: catalist rm [-amfq] [--all] [--remote] REF...\n"));
	CHECK(Run({"help", "search"}) == 0);
	CHECK(Has(out, "Usage: catalist search [-am] [--from=SITE] [--json] [--header] QUERY...\n"));
	CHECK(Has(out, "  pick type title year total score anilist_id mal_id ref kitsu_id\n"));
	CHECK(Run({"help", "stats"}) == 0);
	CHECK(Has(out, "Usage: catalist stats [-am] [--json] [--header]\n"));
	CHECK(Run({"help", "import"}) == 0);
	CHECK(Has(out, "Usage: catalist import [-amq] [--user=NAME] [--from=SITE] [FILE...]\n"));
	CHECK(Run({"help", "export"}) == 0);
	CHECK(Has(out, "Usage: catalist export [--format=FORMAT] [anime|manga]\n"));
	CHECK(Run({"help", "provider"}) == 0);
	CHECK(Has(out, "Usage: catalist provider [anilist|mal|kitsu]\n"));

	CHECK(Run({"help", "frobnicate"}) == 2);
	CHECK(err ==
	      "error: no command 'frobnicate'\n"
	      "Usage: catalist help [COMMAND]\n"
	      "Run 'catalist help help' for more.\n");
	CHECK(out.empty());
	CHECK(Run({"help", "shwo"}) == 2);
	CHECK(err.find("error: no command 'shwo' (did you mean 'show'?)\n") == 0);
	CHECK(Run({"help", "ls", "show"}) == 2);
}

TEST_CASE_FIXTURE(CliFixture, "cli help and version work with a broken catalogue") {
	Put("data/catalist.db", "this is not a catalogue\n");
	CHECK(Run({"--version"}) == 0);
	CHECK(Run({"--help"}) == 0);
	CHECK(Run({"set", "--help"}) == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli points out a mistyped command or option, with exit code 2") {
	CHECK(Run({"lst"}) == 2);
	CHECK(err ==
	      "error: unknown command 'lst' (did you mean 'ls'?)\n"
	      "Usage: catalist [OPTION...] COMMAND [ARG...]\n"
	      "Run 'catalist help' to see the commands.\n");
	CHECK(out.empty());
	CHECK(Run({"frobnicate"}) == 2);
	CHECK(err.find("error: unknown command 'frobnicate'\n") == 0);
	CHECK(Run({"--frobnicate"}) == 2);
	CHECK(err.find("error: unknown option '--frobnicate'\nUsage: catalist [OPTION...] COMMAND") ==
	      0);
	CHECK(Run({"--colour=always", "ls"}) == 2);
	CHECK(err.find("error: unknown option '--colour' (did you mean '--color'?)\n") == 0);
	CHECK(Run({"--color=sometimes", "ls"}) == 2);
	CHECK(err.find("error: --color 'sometimes' is not auto, always or never\n") == 0);
	// The names the earlier command line had are gone.
	for (const char* gone : {"list", "info", "version", "next", "edit", "remove", "delete"})
		CHECK(Run({gone}) == 2);
	CHECK(Run({"udno"}) == 2);
	CHECK(err.find("error: unknown command 'udno' (did you mean 'undo'?)\n") == 0);

	CHECK(Run({"ls", "--jsno"}) == 2);
	CHECK(err ==
	      "error: unknown option '--jsno' (did you mean '--json'?)\n"
	      "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [--linked=SITE] [--unlinked=SITE] "
	      "[--json] [--header] [QUERY...]\n"
	      "Run 'catalist help ls' for more.\n");
	CHECK(Run({"ls", "-ax"}) == 2);
	CHECK(err.find("error: unknown option '-x'\n") == 0);
	CHECK(Run({"ls", "--sort"}) == 2);
	CHECK(err.find("error: option '--sort' needs a value (KEY)\n") == 0);
	CHECK(Run({"ls", "-s"}) == 2);
	CHECK(err.find("error: option '-s' needs a value (STATUS)\n") == 0);
	CHECK(Run({"ls", "--json=yes"}) == 2);
	CHECK(err.find("error: option '--json' takes no value\n") == 0);
	CHECK(Run({"show", "--al", "taxi"}) == 2);
	CHECK(err.find("error: unknown option '--al' (did you mean '--all'?)\n") == 0);
	CHECK(out.empty());

	CHECK(Run({"add", "--stauts", "watching", "frieren"}) == 2);
	CHECK(err.find("error: unknown option '--stauts' (did you mean '--status'?)\n"
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
	for (const auto& args : {std::vector<std::string>{"ls"},
	                         {"ls", "-q"},
	                         {"ls", "--json"},
	                         {"show", "taxi", "berserk"},
	                         {"help"}}) {
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
	CHECK(Run({"show", "taxi", "naruto"}) == 1);
	CHECK(out.empty());
	// A typo names the title it was probably meant to be.
	CHECK(Run({"show", "freiren"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches 'freiren' (did you mean 'Sousou no Frieren', "
	      "a:1?)\n"
	      "See what is there with: catalist ls\n");
	CHECK(Run({"show", "-m", "odd taxo"}) == 1);
	CHECK(err.find("error: nothing in your manga list matches 'odd taxo'\n") == 0);
	CHECK(Run({"show", "odd taxo"}) == 1);
	CHECK(Has(err, "(did you mean 'Odd Taxi', a:2?)"));
	CHECK(Run({"show", "bersek"}) == 1);
	CHECK(Has(err, "(did you mean 'Berserk', m:1?)"));
	// An option after the first title is read as a title.
	CHECK(Run({"show", "taxi", "--json"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches '--json'\n"
	      "Options go before the first operand.\n"
	      "See what is there with: catalist ls\n");
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
	      "   1  a:1  Sousou no Frieren\n"
	      "   2  m:2  Sousou no Frieren\n"
	      "Which one? [1-2, Enter to cancel]: ");
	CHECK(out.find("m:2\t") == 0);
	for (const char* answer : {"\n", "7\n", "both\n"}) {
		CHECK(Run({"show", "frieren"}, answer) == 1);
		CHECK(Has(err, "]: Cancelled.\n"));
		CHECK(out.empty());
	}
	// Standard input that names the titles cannot answer as well.
	CHECK(Run({"show", "-"}, "frieren\n") == 1);
	CHECK(err.find("error: 'frieren' matches 2 titles:\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli says when a command changed nothing, and records no step") {
	Seed();
	// Frieren is complete: there is no more to count.
	CHECK(Run({"+", "a:1"}) == 0);
	CHECK(out.empty());
	CHECK(err == "Nothing changed.\n");
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
	CHECK(Run({"+", "--all", "frieren", "taxi"}) == 0);
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
	for (const char* command : {"stats", "provider"})
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
	// Odd Taxi Has new episodes out, so it comes first.
	CHECK(out.find("Odd Taxi") < out.find("Berserk"));
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli without a command says how to give one, with exit code 2") {
	CHECK(Run({}) == 2);
	CHECK(out.empty());
	CHECK(err ==
	      "error: no command given\nUsage: catalist [OPTION...] COMMAND [ARG...]\n"
	      "Run 'catalist help' to see the commands.\n");
	CHECK_FALSE(std::filesystem::exists("data"));
	// Options alone are no command either.
	CHECK(Run({"--color=never"}) == 2);
	CHECK(err.find("error: no command given\n") == 0);
}

TEST_CASE_FIXTURE(CliFixture, "cli draws tables and colours into a pipe when asked") {
	Seed();
	CHECK(Run({"-t", "ls"}) == 0);
	CHECK(out.find("REF") == 0);
	CHECK(Has(out, "Odd Taxi"));
	CHECK_FALSE(Has(out, "\x1b["));
	CHECK(Run({"--table", "--color=always", "ls", "-a"}) == 0);
	CHECK(Has(out, "\x1b["));
	// --json is what it says, table or not.
	CHECK(Run({"-t", "ls", "--json"}) == 0);
	CHECK(out.find("{\"ref\":") == 0);
	// Colours a terminal would show can be turned off.
	color = true;
	CHECK(Run({"--color=never", "ls"}, "", 100) == 0);
	CHECK_FALSE(Has(out, "\x1b["));
	// The global options come before the command; after it they are its own.
	CHECK(Run({"ls", "-t"}) == 2);
	CHECK(err.find("error: unknown option '-t'\n") == 0);
	// Version and help are global options too.
	CHECK(Run({"-t", "--version"}) == 0);
	CHECK(out.find("Catalist v") == 0);
}
