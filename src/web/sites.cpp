#include "web/sites.hpp"

#include "web/anilist.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

namespace catalist::web {

std::unique_ptr<Tracker> MakeTracker(Provider site, const Link& net) {
	switch (site) {
		case Provider::kMyAnimeList:
			return std::make_unique<mal::Tracker>(net);
		case Provider::kKitsu:
			return std::make_unique<kitsu::Tracker>(net);
		case Provider::kAniList:
			break;
	}
	return std::make_unique<anilist::Tracker>(net);
}

}  // namespace catalist::web
