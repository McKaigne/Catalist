#include "storage/accounts.hpp"

#include <doctest/doctest.h>

#include <filesystem>

#include "support/cli_fixture.hpp"

using catalist::test::CliFixture;
using catalist::test::Has;

TEST_CASE_FIXTURE(CliFixture, "cli login walks through a site's login and keeps the token") {
	ids.anilist = "al-id";
	web.Respond("Viewer", R"({"data":{"Viewer":{"id":5,"name":"Abel"}}})");
	CHECK(Run({"login", "anilist"}, "tok\n") == 0);
	CHECK(err ==
	      "Open this page, let Catalist in, and copy the token AniList shows.\n"
	      "  https://anilist.co/api/v2/oauth/authorize?client_id=al-id&response_type=token\n"
	      "Token: Logged in to AniList as Abel. Each change to a linked title is now sent "
	      "there;\nsend your lists there now with: catalist sync --all\n");
	CHECK(out.empty());
	auto kept = catalist::accounts::Parse(catalist::test::ReadText("state/accounts"));
	REQUIRE(kept.size() == 1);
	CHECK(kept[0].token == "tok");
	// Readable by its owner only, and apart from the lists.
	auto mode = std::filesystem::status("state/accounts").permissions();
	CHECK((mode & std::filesystem::perms::all) ==
	      (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
	CHECK_FALSE(std::filesystem::exists("data"));

	CHECK(Run({"accounts"}) == 0);
	CHECK(out == "anilist\tAbel\tin\t2027-10-20\n");
	CHECK_FALSE(Has(out, "tok"));
	clock += 400LL * 24 * 60 * 60;
	CHECK(Run({"accounts"}, "", 100) == 0);
	CHECK(Has(out, "run out"));

	CHECK(Run({"logout", "anilist"}) == 0);
	CHECK(err == "Logged out of AniList.\n");
	CHECK(Run({"logout", "anilist"}) == 0);
	CHECK(err == "You are not logged in to AniList.\n");
	CHECK(Run({"accounts"}, "", 100) == 0);
	CHECK(err == "You are not logged in anywhere. Log in with: catalist login SITE\n");
}

TEST_CASE_FIXTURE(CliFixture, "cli login says what went wrong, and keeps nothing") {
	CHECK(Run({"login"}) == 2);
	CHECK(err.find("error: name one site: anilist, mal or kitsu\n") == 0);
	CHECK(Run({"login", "anilsit"}) == 2);
	CHECK(err.find("error: unknown site 'anilsit' (did you mean 'anilist'?); choose") == 0);
	// No id from AniList in this build.
	CHECK(Run({"login", "anilist"}, "tok\n") == 1);
	CHECK(Has(err, "CATALIST_ANILIST_CLIENT_ID"));
	// Nothing typed.
	ids.anilist = "al-id";
	CHECK(Run({"login", "anilist"}) == 1);
	CHECK(Has(err, "Cancelled.\n"));
	// AniList cannot be reached.
	CHECK(Run({"login", "anilist"}, "bad\n") == 1);
	CHECK(Has(err, "error: could not reach AniList\n"));
	// A token AniList does not take.
	web.Respond("Viewer", R"({"errors":[{"message":"Invalid token"}]})", 401);
	CHECK(Run({"login", "anilist"}, "bad\n") == 1);
	CHECK(Has(err, "error: AniList did not take the token\n"));
	CHECK_FALSE(std::filesystem::exists("state/accounts"));
}

TEST_CASE_FIXTURE(CliFixture, "cli login to Kitsu asks for the email, then the password") {
	web.Respond("grant_type=password&username=a%40b.c&password=pw",
	            R"({"access_token":"kt","refresh_token":"kr","expires_in":2592000})");
	web.Respond("GET https://kitsu.app/api/edge/users?filter[self]=true",
	            R"({"data":[{"id":"77","attributes":{"name":"abel"}}]})");
	CHECK(Run({"login", "kitsu"}, "a@b.c\npw\n") == 0);
	CHECK(Has(err, "Email: Password: Logged in to Kitsu as abel."));
	CHECK_FALSE(Has(catalist::test::ReadText("state/accounts"), "pw"));
}
