#include "web/tracker.hpp"

namespace catalist::web {

bool unreachable(const std::string& message) {
	return message.rfind("could not reach ", 0) == 0;
}

std::vector<result<details>> iTracker::lookUpMany(const std::vector<entry>& known) {
	std::vector<result<details>> found;
	for (const auto& each : known) {
		if (not found.empty() and not found.back().ok() and
		    unreachable(found.back().errorMessage())) {
			found.push_back(found.back());
			continue;
		}
		if (not found.empty())
			pause();
		found.push_back(lookUp(each));
	}
	return found;
}

}  // namespace catalist::web
