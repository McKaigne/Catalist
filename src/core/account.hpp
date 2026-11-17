#ifndef CATALIST_CORE_ACCOUNT_HPP_
#define CATALIST_CORE_ACCOUNT_HPP_

#include <string>

#include "core/entry.hpp"

namespace catalist {

// A login on a site: what Catalist needs to tell that site what changes in
// the lists. The password never is one of these; the token stands for it.
struct Account {
	Provider site = Provider::kAniList;
	std::string user;
	// The site's number for the user, where its API asks for it (Kitsu).
	std::string user_id;
	std::string token;
	// What gets a new token once this one runs out; empty where the site
	// gives none (AniList).
	std::string renewal;
	// When the token runs out, in seconds since 1970; 0 when not known.
	long long expires = 0;
};

}  // namespace catalist

#endif  // CATALIST_CORE_ACCOUNT_HPP_
