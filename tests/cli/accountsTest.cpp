#include <doctest/doctest.h>

#include <filesystem>

#include "storage/accounts.hpp"
#include "support/cliFixture.hpp"

using catalist::test::cliFixture;
using catalist::test::has;

TEST_CASE_FIXTURE(cliFixture, "cli login walks through a site's login and keeps the token") {
	ids.anilist = "al-id";
	web.respond("Viewer", R"({"data":{"Viewer":{"id":5,"name":"Abel"}}})");
	CHECK(run({"login", "anilist"}, "tok\n") == 0);
	CHECK(err ==
	      "Open this page, let Catalist in, and copy the token AniList shows.\n"
	      "  https://anilist.co/api/v2/oauth/authorize?client_id=al-id&response_type=token\n"
	      "Token: Logged in to AniList as Abel. Each change to a linked title is now sent "
	      "there;\nsend your lists there now with: catalist sync --all\n");
	CHECK(out.empty());
	auto kept = catalist::accounts::parse(catalist::test::readText("state/accounts"));
	REQUIRE(kept.size() == 1);
	CHECK(kept[0].token == "tok");
	// Readable by its owner only, and apart from the lists.
	auto mode = std::filesystem::status("state/accounts").permissions();
	CHECK((mode & std::filesystem::perms::all) ==
	      (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
	CHECK_FALSE(std::filesystem::exists("data"));

	CHECK(run({"accounts"}) == 0);
	CHECK(out == "anilist\tAbel\tin\t2027-10-20\n");
	CHECK_FALSE(has(out, "tok"));
	clock += 400LL * 24 * 60 * 60;
	CHECK(run({"accounts"}, "", 100) == 0);
	CHECK(has(out, "run out"));

	CHECK(run({"logout", "anilist"}) == 0);
	CHECK(err == "Logged out of AniList.\n");
	CHECK(run({"logout", "anilist"}) == 0);
	CHECK(err == "You are not logged in to AniList.\n");
	CHECK(run({"accounts"}, "", 100) == 0);
	CHECK(err == "You are not logged in anywhere. Log in with: catalist login SITE\n");
}

TEST_CASE_FIXTURE(cliFixture, "cli login says what went wrong, and keeps nothing") {
	CHECK(run({"login"}) == 2);
	CHECK(err.find("error: name one site: anilist, mal or kitsu\n") == 0);
	CHECK(run({"login", "anilsit"}) == 2);
	CHECK(err.find("error: unknown site 'anilsit' (did you mean 'anilist'?); choose") == 0);
	// No id from AniList in this build.
	CHECK(run({"login", "anilist"}, "tok\n") == 1);
	CHECK(has(err, "CATALIST_ANILIST_CLIENT_ID"));
	// Nothing typed.
	ids.anilist = "al-id";
	CHECK(run({"login", "anilist"}) == 1);
	CHECK(has(err, "Cancelled.\n"));
	// AniList cannot be reached.
	CHECK(run({"login", "anilist"}, "bad\n") == 1);
	CHECK(has(err, "error: could not reach AniList\n"));
	// A token AniList does not take.
	web.respond("Viewer", R"({"errors":[{"message":"Invalid token"}]})", 401);
	CHECK(run({"login", "anilist"}, "bad\n") == 1);
	CHECK(has(err, "error: AniList did not take the token\n"));
	CHECK_FALSE(std::filesystem::exists("state/accounts"));
}

TEST_CASE_FIXTURE(cliFixture, "cli login to Kitsu asks for the email, then the password") {
	web.respond("grant_type=password&username=a%40b.c&password=pw",
	            R"({"access_token":"kt","refresh_token":"kr","expires_in":2592000})");
	web.respond("GET https://kitsu.app/api/edge/users?filter[self]=true",
	            R"({"data":[{"id":"77","attributes":{"name":"abel"}}]})");
	CHECK(run({"login", "kitsu"}, "a@b.c\npw\n") == 0);
	CHECK(has(err, "Email: Password: Logged in to Kitsu as abel."));
	CHECK_FALSE(has(catalist::test::readText("state/accounts"), "pw"));
}
