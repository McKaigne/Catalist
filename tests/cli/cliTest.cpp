#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/version.hpp"
#include "support/cliFixture.hpp"

using catalist::mediaType;
using catalist::test::cliFixture;
using catalist::test::has;
using catalist::test::kAnimeXml;

TEST_CASE_FIXTURE(cliFixture, "cli --version prints the version") {
	for (const char* flag : {"--version", "-V", "-v"}) {
		CHECK(run({flag}) == 0);
		CHECK(out == std::string("Catalist v") + catalist::kVersionString + "\n");
		CHECK(err.empty());
	}
}

TEST_CASE_FIXTURE(cliFixture, "cli help lists the commands, and each command has its own") {
	for (const char* flag : {"--help", "-h", "help"}) {
		CHECK(run({flag}) == 0);
		CHECK(has(out, "Usage: catalist [OPTION...] COMMAND [ARG...]\n"));
		CHECK(has(out,
		          "\nOptions, before the command:\n"
		          "      --color=WHEN  colours: auto (at a terminal), always or never\n"
		          "  -t, --table       draw tables into a pipe as well ($COLUMNS wide)\n"));
		CHECK(has(out, "  ls          List your titles\n"));
		CHECK(has(out, "  +           Count one more episode or chapter\n"));
		CHECK(has(out, "  undo        Take back what the last command changed\n"));
		CHECK(has(out, "  help        Show help for a command\n"));
		CHECK(has(out, "  add         Add a title found on AniList\n"));
		CHECK(has(out, "  catalist show a:12\n"));
		CHECK(has(out, "kept in\ndata.\n"));
		CHECK(err.empty());
	}
	CHECK(run({"help", "ls"}) == 0);
	CHECK(out.find("List your titles.\n\n"
	               "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [--json] [--header] "
	               "[QUERY...]\n") == 0);
	CHECK(has(out, "ref type title status score progress total volumes rewatched\n"));
	CHECK(has(out, "\nOptions:\n  -a                list anime only\n"));
	CHECK(has(out,
	          "  -s STATUS         only watching, completed, on-hold, dropped, planned, or a,b\n"));
	CHECK(has(out, "      --sort=KEY    new, updated, score, progress, date, status or title\n"));
	CHECK(out.rfind("  -h, --help        show this help\n") + 35 == out.size());
	CHECK(err.empty());
	std::string fromHelp = out;
	for (const char* flag : {"-h", "--help"}) {
		CHECK(run({"ls", flag}) == 0);
		CHECK(out == fromHelp);
	}
	// Asked for among other options, help is all that happens.
	CHECK(run({"ls", "-qh", "--sort=title"}) == 0);
	CHECK(out == fromHelp);
	CHECK(run({"help", "show"}) == 0);
	CHECK(has(out, "Usage: catalist show [-am] [--all] [--json] [--header] REF...\n"));
	CHECK(run({"help", "+"}) == 0);
	CHECK(has(out, "Usage: catalist + [-amq] [--all] [-n N] REF...\nAlso:  catalist inc\n"));
	CHECK(run({"-", "--help"}) == 0);
	CHECK(has(out, "Usage: catalist - [-amq] [--all] [-n N] REF...\nAlso:  catalist dec\n"));
	CHECK(run({"help", "set"}) == 0);
	CHECK(has(out, "Usage: catalist set [-amq] [--all] REF FIELD=VALUE...\n"));
	CHECK(run({"help", "done"}) == 0);
	CHECK(has(out, "Usage: catalist done [-amq] [--all] [--score=N] REF...\n"));
	CHECK(run({"help", "hold"}) == 0);
	CHECK(has(out, "Usage: catalist hold [-amq] [--all] REF...\n"));
	CHECK(run({"help", "undo"}) == 0);
	CHECK(has(out, "Usage: catalist undo [-fq] [-n N]\n"));
	CHECK(run({"help", "tx"}) == 0);
	CHECK(has(out, "export CATALIST_TX=$(catalist tx)\n"));
	CHECK(run({"help", "add"}) == 0);
	CHECK(has(out,
	          "Usage: catalist add [-amq] [--pick=N] [--status=STATUS] [--score=N] "
	          "[--progress=N] [--no-lookup] TITLE...\n"));
	// The helps line up past the longest option.
	CHECK(has(out, "      --status=STATUS  watching, completed, on-hold, dropped or planned\n"));
	CHECK(has(out, "      --no-lookup      add TITLE as written, without asking AniList\n"));
	CHECK(has(out, "  -q                   print nothing\n"));
	CHECK(run({"help", "rm"}) == 0);
	CHECK(has(out, "Usage: catalist rm [-amfq] [--all] REF...\n"));
	CHECK(run({"help", "search"}) == 0);
	CHECK(has(out, "Usage: catalist search [-am] [--json] [--header] QUERY...\n"));
	CHECK(has(out, "  pick type title year total score anilist_id mal_id ref\n"));
	CHECK(run({"help", "stats"}) == 0);
	CHECK(has(out, "Usage: catalist stats [-am] [--json] [--header]\n"));
	CHECK(run({"help", "import"}) == 0);
	CHECK(has(out, "Usage: catalist import [-q] FILE...\n"));
	CHECK(run({"help", "export"}) == 0);
	CHECK(has(out, "Usage: catalist export [--format=FORMAT] [anime|manga]\n"));
	CHECK(run({"help", "provider"}) == 0);
	CHECK(has(out, "Usage: catalist provider [anilist|mal]\n"));

	CHECK(run({"help", "frobnicate"}) == 2);
	CHECK(err ==
	      "error: no command 'frobnicate'\n"
	      "Usage: catalist help [COMMAND]\n"
	      "Run 'catalist help help' for more.\n");
	CHECK(out.empty());
	CHECK(run({"help", "shwo"}) == 2);
	CHECK(err.find("error: no command 'shwo' (did you mean 'show'?)\n") == 0);
	CHECK(run({"help", "ls", "show"}) == 2);
}

TEST_CASE_FIXTURE(cliFixture, "cli names itself as it was run") {
	program = "cl";
	CHECK(run({"help"}) == 0);
	CHECK(has(out, "Usage: cl [OPTION...] COMMAND [ARG...]\n"));
	CHECK(has(out, "  cl show a:12\n"));
	CHECK(has(out, "Run 'cl help COMMAND'"));
	CHECK_FALSE(has(out, "catalist "));
	CHECK(run({"help", "ls"}) == 0);
	CHECK(has(out, "Usage: cl ls [-amrq]"));
	CHECK(run({"lst"}) == 2);
	CHECK(err ==
	      "error: unknown command 'lst' (did you mean 'ls'?)\n"
	      "Usage: cl [OPTION...] COMMAND [ARG...]\n"
	      "Run 'cl help' to see the commands.\n");
	CHECK(run({"ls", "-x"}) == 2);
	CHECK(err ==
	      "error: unknown option '-x'\n"
	      "Usage: cl ls [-amrq] [-s STATUS] [--sort=KEY] [--json] [--header] [QUERY...]\n"
	      "Run 'cl help ls' for more.\n");
	CHECK(run({"show", "naruto"}) == 1);
	CHECK(has(err, "See what is there with: cl ls\n"));
}

TEST_CASE_FIXTURE(cliFixture, "cli says once that it imported the lists of an earlier version") {
	put("data/anime.csv", catalist::test::readText(catalist::test::fixturePath("csv/crlf.csv")));
	CHECK(run({"ls"}) == 0);
	CHECK(has(out, "\tOdd Taxi\t"));
	CHECK(err ==
	      "note: imported the lists in data into data/catalist.db; the old files are left as "
	      "they were.\n");
	CHECK(run({"ls"}) == 0);
	CHECK(has(out, "\tOdd Taxi\t"));
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli help and version work with a broken catalogue") {
	put("data/anime.csv", "this,is,not,a,catalogue\n");
	CHECK(run({"--version"}) == 0);
	CHECK(run({"--help"}) == 0);
	CHECK(run({"set", "--help"}) == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli points out a mistyped command or option, with exit code 2") {
	CHECK(run({"lst"}) == 2);
	CHECK(err ==
	      "error: unknown command 'lst' (did you mean 'ls'?)\n"
	      "Usage: catalist [OPTION...] COMMAND [ARG...]\n"
	      "Run 'catalist help' to see the commands.\n");
	CHECK(out.empty());
	CHECK(run({"frobnicate"}) == 2);
	CHECK(err.find("error: unknown command 'frobnicate'\n") == 0);
	CHECK(run({"--frobnicate"}) == 2);
	CHECK(err.find("error: unknown option '--frobnicate'\nUsage: catalist [OPTION...] COMMAND") ==
	      0);
	CHECK(run({"--colour=always", "ls"}) == 2);
	CHECK(err.find("error: unknown option '--colour' (did you mean '--color'?)\n") == 0);
	CHECK(run({"--color=sometimes", "ls"}) == 2);
	CHECK(err.find("error: --color 'sometimes' is not auto, always or never\n") == 0);
	// The names the earlier command line had are gone.
	for (const char* gone : {"list", "info", "version", "next", "edit", "remove", "delete"})
		CHECK(run({gone}) == 2);
	CHECK(run({"udno"}) == 2);
	CHECK(err.find("error: unknown command 'udno' (did you mean 'undo'?)\n") == 0);

	CHECK(run({"ls", "--jsno"}) == 2);
	CHECK(err ==
	      "error: unknown option '--jsno' (did you mean '--json'?)\n"
	      "Usage: catalist ls [-amrq] [-s STATUS] [--sort=KEY] [--json] [--header] [QUERY...]\n"
	      "Run 'catalist help ls' for more.\n");
	CHECK(run({"ls", "-ax"}) == 2);
	CHECK(err.find("error: unknown option '-x'\n") == 0);
	CHECK(run({"ls", "--sort"}) == 2);
	CHECK(err.find("error: option '--sort' needs a value (KEY)\n") == 0);
	CHECK(run({"ls", "-s"}) == 2);
	CHECK(err.find("error: option '-s' needs a value (STATUS)\n") == 0);
	CHECK(run({"ls", "--json=yes"}) == 2);
	CHECK(err.find("error: option '--json' takes no value\n") == 0);
	CHECK(run({"show", "--al", "taxi"}) == 2);
	CHECK(err.find("error: unknown option '--al' (did you mean '--all'?)\n") == 0);
	CHECK(out.empty());

	CHECK(run({"add", "--stauts", "watching", "frieren"}) == 2);
	CHECK(err.find("error: unknown option '--stauts' (did you mean '--status'?)\n"
	               "Usage: catalist add ") == 0);
	CHECK(has(err, "Run 'catalist help add' for more.\n"));
	CHECK(run({"add", "--pick"}) == 2);
	CHECK(err.find("error: option '--pick' needs a value (N)\n") == 0);
	CHECK(run({"rm", "--yes", "taxi"}) == 2);
	CHECK(run({"import"}) == 2);
	CHECK(err.find("error: give at least one file, or - for standard input\n") == 0);
	CHECK(out.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli stops quietly when nothing reads its output any more") {
	seed();
	outClosed = true;
	for (const auto& args : {std::vector<std::string>{"ls"},
	                         {"ls", "-q"},
	                         {"ls", "--json"},
	                         {"show", "taxi", "berserk"},
	                         {"help"}}) {
		CHECK(run(args) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
	CHECK(run({"ls"}, "", 100) == 0);
	CHECK(run({"show", "--all", "frieren"}, "", 100) == 0);
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli says which title it could not find, and shows none of them") {
	seed();
	CHECK(run({"show", "naruto"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches 'naruto'\n"
	      "See what is there with: catalist ls\n");
	CHECK(run({"show", "-a", "berserk"}) == 1);
	CHECK(err.find("error: nothing in your anime list matches 'berserk'\n") == 0);
	CHECK(run({"show", "a:9"}) == 1);
	CHECK(err.find("error: no title in your lists has the ref a:9\n") == 0);
	CHECK(run({"show", "-m", "a:2"}) == 1);
	CHECK(err.find("error: no title in your manga list has the ref a:2\n") == 0);
	// One REF that fails stops them all.
	CHECK(run({"show", "taxi", "naruto"}) == 1);
	CHECK(out.empty());
	// A typo names the title it was probably meant to be.
	CHECK(run({"show", "freiren"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches 'freiren' (did you mean 'Sousou no Frieren', "
	      "a:1?)\n"
	      "See what is there with: catalist ls\n");
	CHECK(run({"show", "-m", "odd taxo"}) == 1);
	CHECK(err.find("error: nothing in your manga list matches 'odd taxo'\n") == 0);
	CHECK(run({"show", "odd taxo"}) == 1);
	CHECK(has(err, "(did you mean 'Odd Taxi', a:2?)"));
	CHECK(run({"show", "bersek"}) == 1);
	CHECK(has(err, "(did you mean 'Berserk', m:1?)"));
	// An option after the first title is read as a title.
	CHECK(run({"show", "taxi", "--json"}) == 1);
	CHECK(err ==
	      "error: nothing in your lists matches '--json'\n"
	      "Options go before the first operand.\n"
	      "See what is there with: catalist ls\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli asks which title was meant, or lists them into a pipe") {
	seed();
	CHECK(run({"show", "frieren"}) == 1);
	CHECK(err ==
	      "error: 'frieren' matches 2 titles:\n"
	      "  a:1  Sousou no Frieren\n"
	      "  m:2  Sousou no Frieren\n"
	      "Name one by its ref (-a and -m keep to one list), or pass --all for all of them.\n");
	CHECK(out.empty());
	CHECK(run({"show", "-m", "frieren"}) == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK(run({"show", "--all", "frieren"}) == 0);
	CHECK(out.find("a:1\t") == 0);
	CHECK(has(out, "\nm:2\t"));

	CHECK(run({"show", "frieren"}, "2\n") == 0);
	CHECK(err ==
	      "'frieren' matches 2 titles:\n"
	      "   1  a:1  Sousou no Frieren\n"
	      "   2  m:2  Sousou no Frieren\n"
	      "Which one? [1-2, Enter to cancel]: ");
	CHECK(out.find("m:2\t") == 0);
	for (const char* answer : {"\n", "7\n", "both\n"}) {
		CHECK(run({"show", "frieren"}, answer) == 1);
		CHECK(has(err, "]: Cancelled.\n"));
		CHECK(out.empty());
	}
	// Standard input that names the titles cannot answer as well.
	CHECK(run({"show", "-"}, "frieren\n") == 1);
	CHECK(err.find("error: 'frieren' matches 2 titles:\n") == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli says when a command changed nothing, and records no step") {
	seed();
	// Frieren is complete: there is no more to count.
	CHECK(run({"+", "a:1"}) == 0);
	CHECK(out.empty());
	CHECK(err == "Nothing changed.\n");
	CHECK(run({"set", "taxi", "score=8", "progress=5"}) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(run({"resume", "taxi"}) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(run({"done", "a:1"}, "", 100) == 0);
	CHECK(err == "Nothing changed.\n");
	CHECK(out.empty());
	// -q prints nothing, not even that.
	CHECK(run({"+", "-q", "a:1"}) == 0);
	CHECK(err.empty());
	CHECK(steps().empty());
	// Only the titles that changed are printed.
	CHECK(run({"+", "--all", "frieren"}) == 0);
	CHECK(out.find("m:2\t") == 0);
	CHECK_FALSE(has(out, "a:1\t"));
}

TEST_CASE_FIXTURE(cliFixture, "cli stops quietly when nobody reads what a change printed") {
	seed();
	outClosed = true;
	CHECK(run({"+", "--all", "frieren", "taxi"}) == 0);
	CHECK(err.empty());
	CHECK(savedTitle(mediaType::kAnime, "Odd Taxi").progress == 6);
	CHECK(run({"undo"}) == 0);
	CHECK(run({"log"}) == 0);
	CHECK(run({"tx"}) == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli refuses to work on a catalogue it cannot read") {
	put("data/anime.csv", "this,is,not,a,catalogue\n");
	put("lists/anime.xml", kAnimeXml);
	CHECK(run({"import", "lists/anime.xml"}) == 1);
	CHECK(err.find("error: data/anime.csv: ") == 0);
	for (const char* command : {"stats", "provider"})
		CHECK(run({command}) == 1);
	CHECK(run({"export", "anime"}) == 1);
	CHECK(run({"rm", "-f", "taxi"}) == 1);
	CHECK(run({"add", "--no-lookup", "Home Video"}) == 1);
	CHECK(out.empty());
	CHECK(catalist::test::readText("data/anime.csv") == "this,is,not,a,catalogue\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli alone at a terminal lists what is being watched") {
	seed();
	CHECK(run({}, "", 100) == 0);
	CHECK(has(out, "Odd Taxi"));
	CHECK(has(out, "Berserk"));
	CHECK_FALSE(has(out, "Frieren"));
	// Odd Taxi has new episodes out, so it comes first.
	CHECK(out.find("Odd Taxi") < out.find("Berserk"));
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli without a command says how to give one, with exit code 2") {
	program = "cl";
	CHECK(run({}) == 2);
	CHECK(out.empty());
	CHECK(err ==
	      "error: no command given\nUsage: cl [OPTION...] COMMAND [ARG...]\n"
	      "Run 'cl help' to see the commands.\n");
	CHECK_FALSE(std::filesystem::exists("data"));
	// Options alone are no command either.
	CHECK(run({"--color=never"}) == 2);
	CHECK(err.find("error: no command given\n") == 0);
}

TEST_CASE_FIXTURE(cliFixture, "cli draws tables and colours into a pipe when asked") {
	seed();
	CHECK(run({"-t", "ls"}) == 0);
	CHECK(out.find("REF") == 0);
	CHECK(has(out, "Odd Taxi"));
	CHECK_FALSE(has(out, "\x1b["));
	CHECK(run({"--table", "--color=always", "ls", "-a"}) == 0);
	CHECK(has(out, "\x1b["));
	// --json is what it says, table or not.
	CHECK(run({"-t", "ls", "--json"}) == 0);
	CHECK(out.find("{\"ref\":") == 0);
	// Colours a terminal would show can be turned off.
	color = true;
	CHECK(run({"--color=never", "ls"}, "", 100) == 0);
	CHECK_FALSE(has(out, "\x1b["));
	// The global options come before the command; after it they are its own.
	CHECK(run({"ls", "-t"}) == 2);
	CHECK(err.find("error: unknown option '-t'\n") == 0);
	// Version and help are global options too.
	CHECK(run({"-t", "--version"}) == 0);
	CHECK(out.find("Catalist v") == 0);
}
