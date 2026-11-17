#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "support/cli_fixture.hpp"

using catalist::test::CliFixture;
using catalist::test::Has;

TEST_CASE_FIXTURE(CliFixture, "cli provider prints or sets the site titles are looked at on") {
	Seed();
	CHECK(Run({"provider"}) == 0);
	CHECK(out == "anilist\n");
	CHECK(err.empty());
	CHECK(Run({"provider", "mal"}) == 0);
	CHECK(out.empty());
	CHECK(err == "Your provider is now MyAnimeList.\n");
	{
		catalist::Catalogue db("data");
		REQUIRE(db.LoadAll().has_value());
		CHECK(db.provider() == catalist::Provider::kMyAnimeList);
	}
	CHECK(Run({"provider"}) == 0);
	CHECK(out == "mal\n");
	CHECK(Run({"show", "taxi"}) == 0);
	CHECK(Has(out, "\thttps://myanimelist.net/anime/44265\t0\n"));
	CHECK(Run({"show", "taxi"}, "", 100) == 0);
	CHECK(Has(out, "  Page         https://myanimelist.net/anime/44265\n"));
	CHECK(Run({"provider", "myanimelist"}) == 0);
	CHECK(err == "Nothing changed.\n");

	CHECK(Run({"provider", "shikimori"}) == 2);
	CHECK(err.find("error: unknown provider 'shikimori'; choose anilist, mal or kitsu\n"
	               "Usage: catalist provider [anilist|mal|kitsu]\n") == 0);
	CHECK(Run({"provider", "kitsu"}) == 0);
	CHECK(err == "Your provider is now Kitsu.\n");
	CHECK(Run({"provider"}) == 0);
	CHECK(out == "kitsu\n");
	CHECK(Run({"provider", "anilist", "mal"}) == 2);
	CHECK(Run({"provider", "anilist"}) == 0);
	CHECK(Run({"show", "taxi"}) == 0);
	CHECK(Has(out, "\thttps://anilist.co/anime/128547\t0\n"));
	// A setting is not a step for undo.
	CHECK(Steps().empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli completion prints the script of the shell named") {
	CHECK(Run({"completion", "zsh"}) == 0);
	CHECK(out.rfind("#compdef catalist\n", 0) == 0);
	CHECK(Has(out, "'1:command:_catalist_commands'"));
	CHECK(err.empty());
	CHECK(Run({"completion", "fish"}) == 0);
	CHECK(Has(out, "complete -c catalist -f\n"));
	CHECK(Has(out, "-a 'ls' -d 'List your titles'\n"));
	// The scripts are the program's own: they make no lists.
	CHECK_FALSE(std::filesystem::exists("data"));

	for (std::vector<std::string> wrong : {std::vector<std::string>{"completion"},
	                                       {"completion", "bash"},
	                                       {"completion", "zsh", "fish"}}) {
		CHECK(Run(wrong) == 2);
		CHECK(out.empty());
		CHECK(Has(err, "error: say which shell: zsh or fish\n"));
	}
}

TEST_CASE_FIXTURE(CliFixture, "cli __complete prints the refs and titles for the scripts") {
	Seed();
	CHECK(Run({"__complete", "refs"}) == 0);
	CHECK(out == "a:1\tSousou no Frieren\na:2\tOdd Taxi\nm:1\tBerserk\nm:2\tSousou no Frieren\n");
	CHECK(err.empty());
	CHECK(Run({"__complete", "titles"}) == 0);
	CHECK(out == "Sousou no Frieren\ta:1\nOdd Taxi\ta:2\nBerserk\tm:1\nSousou no Frieren\tm:2\n");
	CHECK(Run({"__complete", "statuses"}) == 0);
	CHECK(out == "watching\ncompleted\non-hold\ndropped\nplanned\nrewatching\n");
	CHECK(Run({"__complete", "fields"}) == 0);
	CHECK(out.rfind("total=\nprogress=\nstatus=\nscore=\n", 0) == 0);
	CHECK(Has(out, "start=\nfinish=\n"));
	// It is not one of the commands the person is shown.
	CHECK(Run({"help"}) == 0);
	CHECK_FALSE(Has(out, "__complete"));
	CHECK_FALSE(Has(out, "__man"));
	CHECK_FALSE(Has(out, "__readme"));
	CHECK(Run({"completion", "zsh"}) == 0);
	CHECK_FALSE(Has(out, "__complete)"));
}

TEST_CASE_FIXTURE(CliFixture, "cli __complete says nothing and exits 0 whatever goes wrong") {
	// No lists yet: nothing is made.
	for (const char* what : {"refs", "titles"}) {
		CHECK(Run({"__complete", what}) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
	CHECK_FALSE(std::filesystem::exists("data/catalist.db"));
	// Lists it cannot read.
	Put("data/catalist.db", "not a database");
	CHECK(Run({"__complete", "refs"}) == 0);
	CHECK(out.empty());
	CHECK(err.empty());
	// A word, an option or an operand it does not know.
	for (std::vector<std::string> wrong : {std::vector<std::string>{"__complete"},
	                                       {"__complete", "nope"},
	                                       {"__complete", "--nope"},
	                                       {"__complete", "-x", "refs"}}) {
		CHECK(Run(wrong) == 0);
		CHECK(out.empty());
		CHECK(err.empty());
	}
}

TEST_CASE_FIXTURE(CliFixture, "cli __man prints the commands for the manual page") {
	CHECK(Run({"__man"}) == 0);
	CHECK(out.rfind(".TP\n\\fBls\\fR ", 0) == 0);
	CHECK(Has(out, "\n.TP\n\\fBcompletion\\fR \\fBzsh\\fR|\\fBfish\\fR\n"));
	CHECK_FALSE(Has(out, "__complete"));
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli __readme prints the commands for the README") {
	CHECK(Run({"__readme"}) == 0);
	CHECK(out.rfind("- `catalist ls ", 0) == 0);
	CHECK(
	    Has(out, "\n- `catalist completion zsh|fish`: Print the completion script of a shell.\n"));
	CHECK_FALSE(Has(out, "__complete"));
	CHECK(err.empty());
}

TEST_CASE_FIXTURE(CliFixture, "cli takes every value the scripts offer") {
	Seed();
	for (const char* key : {"new", "updated", "score", "progress", "date", "status", "title"}) {
		CAPTURE(key);
		CHECK(Run({"ls", std::string("--sort=") + key}) == 0);
	}
	for (const char* status : {"watching", "completed", "on-hold", "dropped", "planned"}) {
		CAPTURE(status);
		CHECK(Run({"ls", "-s", status}) == 0);
	}
	for (const char* format : {"xml", "csv", "json"}) {
		CAPTURE(format);
		CHECK(Run({"export", std::string("--format=") + format, "anime"}) == 0);
	}
}
