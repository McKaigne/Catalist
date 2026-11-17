#include <doctest/doctest.h>

#include "support/testSupport.hpp"
#include "web/anilist.hpp"
#include "web/http.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

using catalist::provider;
namespace web = catalist::web;

namespace {

// A link with the sites' ids and a fixed time, through the fake web.
web::link linkWith(catalist::test::fakeWeb& fake) {
	auto net = fake.link();
	net.ids = {"al-id", "mal-id", "", ""};
	net.now = [] { return 1000LL; };
	return net;
}

}  // namespace

TEST_CASE("web encodes forms and reads the query of an address") {
	CHECK(web::formEncoded({{"grant_type", "password"}, {"username", "a b@c.d"}}) ==
	      "grant_type=password&username=a%20b%40c.d");
	CHECK(web::queryValue("http://localhost/cb?code=a%2Fb&state=catalist", "code") == "a/b");
	CHECK(web::queryValue("http://localhost/cb?state=x&code=z#frag", "code") == "z");
	CHECK_FALSE(web::queryValue("http://localhost/cb?state=x", "code"));
	CHECK_FALSE(web::queryValue("just-a-code", "code"));
}

TEST_CASE("anilist logs in with the token its page shows") {
	catalist::test::fakeWeb fake;
	catalist::anilist::tracker site(linkWith(fake));
	auto start = site.startLogin();
	REQUIRE(start.has_value());
	CHECK(start.value().page ==
	      "https://anilist.co/api/v2/oauth/authorize?client_id=al-id&response_type=token");
	REQUIRE(start.value().questions.size() == 1);
	CHECK(start.value().questions[0].secret);

	fake.respond("Viewer", R"({"data":{"Viewer":{"id":5,"name":"Abel",
	  "mediaListOptions":{"scoreFormat":"POINT_100"}}}})");
	auto login = site.finishLogin({"tok"});
	REQUIRE(login.has_value());
	CHECK(login.value().user == "Abel");
	CHECK(login.value().scoreFormat == "POINT_100");
	CHECK(login.value().userId == "5");
	CHECK(login.value().token == "tok");
	CHECK(login.value().expires == 1000 + 365LL * 24 * 60 * 60);
	// A login good for a year is not renewed; one that has run out cannot be.
	CHECK(site.current(login.value()).has_value());
	auto old = login.value();
	old.expires = 1030;
	CHECK_FALSE(site.current(old).has_value());

	catalist::test::fakeWeb offline;
	catalist::anilist::tracker unreached(linkWith(offline));
	CHECK_FALSE(unreached.finishLogin({"tok"}).has_value());
	auto bare = offline.link();
	CHECK(catalist::anilist::tracker(bare).startLogin().error().message.find(
	          "CATALIST_ANILIST_CLIENT_ID") != std::string::npos);
}

TEST_CASE("mal logs in with PKCE and the address the browser was sent to") {
	catalist::test::fakeWeb fake;
	catalist::mal::tracker site(linkWith(fake));
	auto start = site.startLogin();
	REQUIRE(start.has_value());
	std::string verifier(64, 'x');
	CHECK(start.value().page.find("client_id=mal-id&code_challenge=" + verifier +
	                              "&code_challenge_method=plain") != std::string::npos);

	fake.respond(
	    "POST https://myanimelist.net/v1/oauth2/token client_id=mal-id"
	    "&grant_type=authorization_code&code=C0DE&code_verifier=" +
	        verifier,
	    R"({"token_type":"Bearer","expires_in":2678400,"access_token":"at",
	             "refresh_token":"rt"})");
	fake.respond("GET https://api.myanimelist.net/v2/users/@me", R"({"id":9,"name":"Abel"})");
	auto login = site.finishLogin({"http://localhost/catalist?code=C0DE&state=catalist"});
	REQUIRE(login.has_value());
	CHECK(login.value().site == provider::kMyAnimeList);
	CHECK(login.value().user == "Abel");
	CHECK(login.value().token == "at");
	CHECK(login.value().renewal == "rt");
	CHECK(login.value().expires == 1000 + 2678400);

	// Running out, it is renewed with the refresh token.
	fake.respond("grant_type=refresh_token&refresh_token=rt",
	             R"({"expires_in":100,"access_token":"at2","refresh_token":"rt2"})");
	auto old = login.value();
	old.expires = 1010;
	auto renewed = site.current(old);
	REQUIRE(renewed.has_value());
	CHECK(renewed.value().token == "at2");
	CHECK(renewed.value().renewal == "rt2");
}

TEST_CASE("kitsu logs in with email and password, for a token it renews") {
	catalist::test::fakeWeb fake;
	catalist::kitsu::tracker site(linkWith(fake));
	auto start = site.startLogin();
	REQUIRE(start.has_value());
	CHECK(start.value().page.empty());
	REQUIRE(start.value().questions.size() == 2);
	CHECK_FALSE(start.value().questions[0].secret);
	CHECK(start.value().questions[1].secret);

	fake.respond(
	    "POST https://kitsu.app/api/oauth/token grant_type=password&username=a%40b.c"
	    "&password=p%20w",
	    R"({"access_token":"kt","refresh_token":"kr","expires_in":2592000})");
	fake.respond("GET https://kitsu.app/api/edge/users?filter[self]=true",
	             R"({"data":[{"id":"77","attributes":{"name":"abel","ratingSystem":"regular"}}]})");
	auto login = site.finishLogin({"a@b.c", "p w"});
	REQUIRE(login.has_value());
	CHECK(login.value().user == "abel");
	CHECK(login.value().scoreFormat == "regular");
	CHECK(login.value().userId == "77");
	CHECK(login.value().token == "kt");
	// The password is in no field of the account.
	for (const auto& field :
	     {login.value().user, login.value().userId, login.value().token, login.value().renewal})
		CHECK(field.find("p w") == std::string::npos);

	fake.respond("grant_type=password&username=a%40b.c&password=wrong",
	             R"({"error":"invalid_grant"})", 400);
	CHECK(site.finishLogin({"a@b.c", "wrong"}).error().message.find("did not take") !=
	      std::string::npos);
}
