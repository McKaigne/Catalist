#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "support/cliFixture.hpp"

using catalist::test::cliFixture;
using catalist::test::has;

TEST_CASE_FIXTURE(cliFixture, "cli provider prints or sets the site titles are looked at on") {
	seed();
	CHECK(run({"provider"}) == 0);
	CHECK(out == "anilist\n");
	CHECK(err.empty());
	CHECK(run({"provider", "mal"}) == 0);
	CHECK(out.empty());
	CHECK(err == "Your provider is now MyAnimeList.\n");
	{
		catalist::catalogue db("data");
		REQUIRE(db.loadAll().has_value());
		CHECK(db.getProvider() == catalist::provider::kMyAnimeList);
	}
	CHECK(run({"provider"}) == 0);
	CHECK(out == "mal\n");
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(has(out, "\thttps://myanimelist.net/anime/44265\t0\n"));
	CHECK(run({"show", "taxi"}, "", 100) == 0);
	CHECK(has(out, "  Page         https://myanimelist.net/anime/44265\n"));
	CHECK(run({"provider", "myanimelist"}) == 0);
	CHECK(err == "Nothing changed.\n");

	CHECK(run({"provider", "shikimori"}) == 2);
	CHECK(err.find("error: unknown provider 'shikimori'; choose anilist, mal or kitsu\n"
	               "Usage: catalist provider [anilist|mal|kitsu]\n") == 0);
	CHECK(run({"provider", "kitsu"}) == 0);
	CHECK(err == "Your provider is now Kitsu.\n");
	CHECK(run({"provider"}) == 0);
	CHECK(out == "kitsu\n");
	CHECK(run({"provider", "anilist", "mal"}) == 2);
	CHECK(run({"provider", "anilist"}) == 0);
	CHECK(run({"show", "taxi"}) == 0);
	CHECK(has(out, "\thttps://anilist.co/anime/128547\t0\n"));
	// A setting is not a step for undo.
	CHECK(steps().empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli completion prints the script of the shell named") {
	CHECK(run({"completion", "zsh"}) == 0);
	CHECK(out.rfind("#compdef catalist\n", 0) == 0);
	CHECK(has(out, "'1:command:_catalist_commands'"));
	CHECK(err.empty());
	CHECK(run({"completion", "fish"}) == 0);
	CHECK(has(out, "complete -c catalist -f\n"));
	CHECK(has(out, "-a 'ls' -d 'List your titles'\n"));
	// The scripts are the program's own: they make no lists.
	CHECK_FALSE(std::filesystem::exists("data"));

	for (std::vector<std::string> wrong : {std::vector<std::string>{"completion"},
	                                       {"completion", "bash"},
	                                       {"completion", "zsh", "fish"}}) {
		CHECK(run(wrong) == 2);
		CHECK(out.empty());
		CHECK(has(err, "error: say which shell: zsh or fish\n"));
	}
}

TEST_CASE_FIXTURE(cliFixture, "cli __complete prints the refs and titles for the scripts") {
	seed();
	CHECK(run({"__complete", "refs"}) == 0);
	CHECK(out == "a:1\tSousou no Frieren\na:2\tOdd Taxi\nm:1\tBerserk\nm:2\tSousou no Frieren\n");
	CHECK(err.empty());
	CHECK(run({"__complete", "titles"}) == 0);
	CHECK(out == "Sousou no Frieren\ta:1\nOdd Taxi\ta:2\nBerserk\tm:1\nSousou no Frieren\tm:2\n");
	CHECK(run({"__complete", "statuses"}) == 0);
	CHECK(out == "watching\ncompleted\non-hold\ndropped\nplanned\n");
	CHECK(run({"__complete", "fields"}) == 0);
	CHECK(out.rfind("total=\nprogress=\nstatus=\nscore=\n", 0) == 0);
	CHECK(has(out, "start=\nfinish=\n"));
	// It is not one of the commands the person is shown.
	CHECK(run({"help"}) == 0);
	CHECK_FALSE(has(out, "__complete"));
	CHECK_FALSE(has(out, "__man"));
	CHECK(run({"completion", "zsh"}) == 0);
	CHECK_FALSE(has(out, "__complete)"));
}

TEST_CASE_FIXTURE(cliFixture, "cli __complete says nothing and exits 0 whatever goes wrong") {
	// No lists yet: nothing is made.
	for (const char* what : {"refs", "titles"}) {
		CHECK(run({"__complete", what}) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
	CHECK_FALSE(std::filesystem::exists("data/catalist.db"));
	// Lists it cannot read.
	put("data/catalist.db", "not a database");
	CHECK(run({"__complete", "refs"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	// A word, an option or an operand it does not know.
	for (std::vector<std::string> wrong : {std::vector<std::string>{"__complete"},
	                                       {"__complete", "nope"},
	                                       {"__complete", "--nope"},
	                                       {"__complete", "-x", "refs"}}) {
		CHECK(run(wrong) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
}

TEST_CASE_FIXTURE(cliFixture, "cli __man prints the commands for the manual page") {
	CHECK(run({"__man"}) == 0);
	CHECK(out.rfind(".TP\n\\fBls\\fR ", 0) == 0);
	CHECK(has(out, "\n.TP\n\\fBcompletion\\fR \\fBzsh\\fR|\\fBfish\\fR\n"));
	CHECK_FALSE(has(out, "__complete"));
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(cliFixture, "cli takes every value the scripts offer") {
	seed();
	for (const char* key : {"new", "updated", "score", "progress", "date", "status", "title"}) {
		CAPTURE(key);
		CHECK(run({"ls", std::string("--sort=") + key}) == 0);
	}
	for (const char* status : {"watching", "completed", "on-hold", "dropped", "planned"}) {
		CAPTURE(status);
		CHECK(run({"ls", "-s", status}) == 0);
	}
	for (const char* format : {"xml", "csv", "json"}) {
		CAPTURE(format);
		CHECK(run({"export", std::string("--format=") + format, "anime"}) == 0);
	}
}
