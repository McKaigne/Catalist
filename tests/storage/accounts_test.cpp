#include "storage/accounts.hpp"

#include <doctest/doctest.h>

namespace accounts = catalist::accounts;
using catalist::Account;
using catalist::Provider;

TEST_CASE("accounts are one line a site, and read back as written") {
	std::vector<Account> logins = {
	    {Provider::kAniList, "Abel", "5", "tok.en", "", 1800000000},
	    {Provider::kKitsu, "abel", "77", "kt", "rt", 0},
	};
	std::string text = accounts::Format(logins);
	CHECK(text == "anilist\tAbel\t5\ttok.en\t\t1800000000\nkitsu\tabel\t77\tkt\trt\t0\n");
	auto read = accounts::Parse(text);
	REQUIRE(read.size() == 2);
	CHECK(read[1].site == Provider::kKitsu);
	CHECK(read[1].renewal == "rt");
	CHECK(read[0].expires == 1800000000);
}

TEST_CASE("accounts leave out what they cannot read, and keep one login a site") {
	auto read = accounts::Parse(
	    "shikimori\ta\t1\tt\t\t0\n"
	    "mal\tonly three\tfields\n"
	    "mal\tAbel\t1\t\t\t0\n"
	    "mal\tAbel\t1\tfirst\t\tsoon\n"
	    "mal\tOther\t2\tsecond\t\t0\n");
	REQUIRE(read.size() == 1);
	CHECK(read[0].token == "first");
	CHECK(read[0].expires == 0);
	// A field that would split the line is not written.
	CHECK(accounts::Format({{Provider::kMyAnimeList, "a\tb", "", "t", "", 0}}).empty());
}

TEST_CASE("accounts replace and forget the login of one site") {
	std::vector<Account> logins = {{Provider::kAniList, "old", "", "t1", "", 0}};
	logins = accounts::With(logins, {Provider::kAniList, "new", "", "t2", "", 0});
	logins = accounts::With(logins, {Provider::kKitsu, "k", "", "t3", "", 0});
	REQUIRE(logins.size() == 2);
	CHECK(accounts::On(logins, Provider::kAniList)->user == "new");
	logins = accounts::Without(logins, Provider::kAniList);
	CHECK_FALSE(accounts::On(logins, Provider::kAniList));
	CHECK(accounts::On(logins, Provider::kKitsu));
}

TEST_CASE("accounts live apart from the lists' directory unless it is named") {
	CHECK(accounts::DefaultPath("/d", "/s", "/h") == "/d/accounts");
	CHECK(accounts::DefaultPath("", "/s", "/h") == "/s/catalist/accounts");
	CHECK(accounts::DefaultPath("", "relative", "/h") == "/h/.local/state/catalist/accounts");
}
