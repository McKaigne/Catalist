#include "web/sites.hpp"

#include "web/anilist.hpp"
#include "web/kitsu.hpp"
#include "web/mal.hpp"

namespace catalist::web {

std::unique_ptr<iTracker> makeTracker(provider site, const link& net) {
	switch (site) {
		case provider::kMyAnimeList:
			return std::make_unique<mal::tracker>(net);
		case provider::kKitsu:
			return std::make_unique<kitsu::tracker>(net);
		case provider::kAniList:
			break;
	}
	return std::make_unique<anilist::tracker>(net);
}

}  // namespace catalist::web
