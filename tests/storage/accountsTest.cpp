#include <doctest/doctest.h>

#include "storage/accounts.hpp"

namespace accounts = catalist::accounts;
using catalist::account;
using catalist::provider;

TEST_CASE("accounts are one line a site, and read back as written") {
	std::vector<account> logins = {
	    {provider::kAniList, "Abel", "5", "tok.en", "", 1800000000, "POINT_100"},
	    {provider::kKitsu, "abel", "77", "kt", "rt", 0, "simple"},
	};
	std::string text = accounts::format(logins);
	CHECK(text ==
	      "anilist\tAbel\t5\ttok.en\t\t1800000000\tPOINT_100\n"
	      "kitsu\tabel\t77\tkt\trt\t0\tsimple\n");
	auto read = accounts::parse(text);
	REQUIRE(read.size() == 2);
	CHECK(read[1].site == provider::kKitsu);
	CHECK(read[1].renewal == "rt");
	CHECK(read[1].scoreFormat == "simple");
	CHECK(read[0].expires == 1800000000);
	// A file written before the score format was kept still reads.
	auto older = accounts::parse("anilist\tAbel\t5\ttok.en\t\t1800000000\n");
	REQUIRE(older.size() == 1);
	CHECK(older[0].scoreFormat.empty());
}

TEST_CASE("accounts leave out what they cannot read, and keep one login a site") {
	auto read = accounts::parse(
	    "shikimori\ta\t1\tt\t\t0\n"
	    "mal\tonly three\tfields\n"
	    "mal\tAbel\t1\t\t\t0\n"
	    "mal\tAbel\t1\tfirst\t\tsoon\n"
	    "mal\tOther\t2\tsecond\t\t0\n");
	REQUIRE(read.size() == 1);
	CHECK(read[0].token == "first");
	CHECK(read[0].expires == 0);
	// A field that would split the line is not written.
	CHECK(accounts::format({{provider::kMyAnimeList, "a\tb", "", "t", "", 0}}).empty());
}

TEST_CASE("accounts replace and forget the login of one site") {
	std::vector<account> logins = {{provider::kAniList, "old", "", "t1", "", 0}};
	logins = accounts::with(logins, {provider::kAniList, "new", "", "t2", "", 0});
	logins = accounts::with(logins, {provider::kKitsu, "k", "", "t3", "", 0});
	REQUIRE(logins.size() == 2);
	CHECK(accounts::on(logins, provider::kAniList)->user == "new");
	logins = accounts::without(logins, provider::kAniList);
	CHECK_FALSE(accounts::on(logins, provider::kAniList));
	CHECK(accounts::on(logins, provider::kKitsu));
}

TEST_CASE("accounts live apart from the lists' directory unless it is named") {
	CHECK(accounts::defaultPath("/d", "/s", "/h") == "/d/accounts");
	CHECK(accounts::defaultPath("", "/s", "/h") == "/s/catalist/accounts");
	CHECK(accounts::defaultPath("", "relative", "/h") == "/h/.local/state/catalist/accounts");
}
