#include "updater.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <tuple>

namespace catalist {

namespace {

// How long quitting waits for a request under way.
constexpr auto kStopWait = std::chrono::milliseconds(200);

// A title in the updater's maps: its AniList id, or minus its MyAnimeList id
// while the AniList one is not known.
using titleKey = std::pair<mediaType, int>;
using coverKey = std::tuple<mediaType, int, int, int>;

titleKey keyOf(const anilist::titleRef& title) {
	return {title.type, title.anilistId > 0 ? title.anilistId : -title.malId};
}

struct job {
	enum class kind { kLookup, kCover, kLink } what = kind::kLookup;
	anilist::titleRef title;
	int widthPx = 0;
	int heightPx = 0;
	// For a link: the ids to ask by, and the site they are from.
	provider known = provider::kAniList;
	std::vector<int> ids;
};

}  // namespace

struct updater::shared {
	shared(fetcher fetch, anilist::poster post, std::function<isoDate()> today, sixelMaker toSixel,
	       std::chrono::milliseconds pause, std::function<void()> wake,
	       std::chrono::milliseconds retryAfter)
	    : fetch(std::move(fetch)),
	      post(std::move(post)),
	      today(std::move(today)),
	      toSixel(std::move(toSixel)),
	      pause(pause),
	      wake(std::move(wake)),
	      retryAfter(retryAfter) {}

	void work();
	void runLookup(const anilist::titleRef& title);
	void runCover(const job& next);
	void runLink(const job& next);
	// Waits until the pause since the last request is over; false when the
	// updater is stopping instead.
	bool waitTurn();
	// Asks AniList, then notes the AniList id of a title known only by its
	// MyAnimeList id, so both name the same title from then on.
	result<anilist::details> lookUpPaced(const anilist::titleRef& title);
	// The cover's address: known, looked up, absent because there is none
	// (an empty string), or nothing when AniList could not be reached.
	std::optional<std::string> imageUrlOf(const anilist::titleRef& title);
	// The key every map uses for the title (needs the lock).
	titleKey canonical(const anilist::titleRef& title) const;

	const fetcher fetch;
	const anilist::poster post;
	const std::function<isoDate()> today;
	const sixelMaker toSixel;
	const std::chrono::milliseconds pause;
	const std::function<void()> wake;
	const std::chrono::milliseconds retryAfter;

	// Everything below is guarded by mutex.
	std::mutex mutex;
	std::condition_variable changed;
	std::deque<job> jobs;
	bool working = false;
	bool stopping = false;
	bool done = false;
	std::chrono::steady_clock::time_point lastRequest;
	std::map<titleKey, titleKey> aliases;  // MyAnimeList key to AniList key
	std::set<titleKey> lookedUp;
	std::vector<finding> findings;
	std::vector<link> links;
	std::map<titleKey, std::string> imageUrls;
	std::map<titleKey, std::string> images;
	// Why a title has no cover, and when an unreachable one may be retried.
	std::map<titleKey, coverState> failed;
	std::map<titleKey, std::chrono::steady_clock::time_point> retryAt;
	std::set<coverKey> asked;
	std::map<coverKey, std::string> sixels;
};

updater::updater(fetcher fetch, anilist::poster post, std::function<isoDate()> today,
                 sixelMaker toSixel, std::chrono::milliseconds pause, std::function<void()> wake,
                 std::chrono::milliseconds retryAfter)
    : shared_(std::make_shared<shared>(std::move(fetch), std::move(post), std::move(today),
                                       std::move(toSixel), pause, std::move(wake), retryAfter)) {
	worker_ = std::thread([state = shared_] { state->work(); });
}

updater::~updater() {
	std::unique_lock<std::mutex> lock(shared_->mutex);
	shared_->stopping = true;
	shared_->changed.notify_all();
	// A request under way may take seconds (curl allows eight). After a
	// moment the worker is left to finish it alone; it holds its own share
	// of the state and calls nothing outside it once stopping is set.
	bool finished = shared_->changed.wait_for(lock, kStopWait, [this] { return shared_->done; });
	lock.unlock();
	if (finished)
		worker_.join();
	else
		worker_.detach();
}

void updater::lookUp(const anilist::titleRef& title) {
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		bool hasId = title.anilistId > 0 or title.malId > 0;
		if (not hasId or not shared_->lookedUp.insert(shared_->canonical(title)).second)
			return;
		job lookup;
		lookup.title = title;
		shared_->jobs.push_back(lookup);
	}
	shared_->changed.notify_all();
}

std::vector<updater::finding> updater::takeFindings() {
	std::lock_guard<std::mutex> lock(shared_->mutex);
	return std::exchange(shared_->findings, {});
}

updater::cover updater::coverFor(const anilist::titleRef& title, int widthPx, int heightPx) {
	if (title.anilistId <= 0 and title.malId <= 0)
		return {coverState::kNone, ""};
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		titleKey name = shared_->canonical(title);
		coverKey key{name.first, name.second, widthPx, heightPx};
		auto ready = shared_->sixels.find(key);
		if (ready != shared_->sixels.end())
			return {coverState::kReady, ready->second};

		// A failure stands, except that an unreachable cover is asked for
		// again once its wait is over.
		auto failure = shared_->failed.find(name);
		if (failure != shared_->failed.end()) {
			bool retry = failure->second == coverState::kUnreachable and
			             std::chrono::steady_clock::now() >= shared_->retryAt[name];
			if (not retry)
				return {failure->second, ""};
			shared_->failed.erase(failure);
			shared_->asked.erase(key);
		}
		if (not shared_->asked.insert(key).second)
			return {coverState::kLoading, ""};
		// The person is looking at this one, so it goes before any lookups.
		job cover;
		cover.what = job::kind::kCover;
		cover.title = title;
		cover.widthPx = widthPx;
		cover.heightPx = heightPx;
		shared_->jobs.push_front(cover);
	}
	shared_->changed.notify_all();
	return {coverState::kLoading, ""};
}

void updater::linkIds(mediaType type, provider known, std::vector<int> ids) {
	if (ids.empty())
		return;
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		job link;
		link.what = job::kind::kLink;
		link.title.type = type;
		link.known = known;
		link.ids = std::move(ids);
		shared_->jobs.push_back(std::move(link));
	}
	shared_->changed.notify_all();
}

std::vector<updater::link> updater::takeLinks() {
	std::lock_guard<std::mutex> lock(shared_->mutex);
	return std::exchange(shared_->links, {});
}

void updater::waitUntilIdle() {
	std::unique_lock<std::mutex> lock(shared_->mutex);
	shared_->changed.wait(lock, [this] { return shared_->jobs.empty() and not shared_->working; });
}

titleKey updater::shared::canonical(const anilist::titleRef& title) const {
	titleKey key = keyOf(title);
	auto alias = aliases.find(key);
	return alias == aliases.end() ? key : alias->second;
}

void updater::shared::work() {
	while (true) {
		// Take the next job, or stop.
		job next;
		{
			std::unique_lock<std::mutex> lock(mutex);
			changed.wait(lock, [this] { return stopping or not jobs.empty(); });
			if (stopping) {
				done = true;
				changed.notify_all();
				return;
			}
			next = jobs.front();
			jobs.pop_front();
			working = true;
		}

		// The requests run without the lock, so the interface never waits.
		switch (next.what) {
			case job::kind::kLookup:
				runLookup(next.title);
				break;
			case job::kind::kCover:
				runCover(next);
				break;
			case job::kind::kLink:
				runLink(next);
				break;
		}

		// Report back; the interface is only woken while it still exists.
		std::lock_guard<std::mutex> lock(mutex);
		working = false;
		changed.notify_all();
		if (wake and not stopping)
			wake();
	}
}

bool updater::shared::waitTurn() {
	std::unique_lock<std::mutex> lock(mutex);
	changed.wait_until(lock, lastRequest + pause, [this] { return stopping; });
	if (stopping)
		return false;
	lastRequest = std::chrono::steady_clock::now();
	return true;
}

result<anilist::details> updater::shared::lookUpPaced(const anilist::titleRef& title) {
	auto pacedPost = [this](const std::string& url, const std::string& json) {
		return waitTurn() ? post(url, json) : std::nullopt;
	};
	auto found = anilist::lookUp(pacedPost, title);
	if (not found.ok())
		return found;
	std::lock_guard<std::mutex> lock(mutex);
	titleKey asked = keyOf(title);
	titleKey own{title.type, found.value().facts.anilistId};
	if (own.second > 0 and asked != own) {
		aliases[asked] = own;
		// What was kept under the old key now belongs to the AniList one.
		for (auto* known : {&imageUrls, &images}) {
			auto kept = known->find(asked);
			if (kept != known->end())
				(*known)[own] = kept->second;
		}
		if (lookedUp.count(asked))
			lookedUp.insert(own);
	}
	imageUrls[own.second > 0 ? own : asked] = found.value().imageUrl;
	return found;
}

void updater::shared::runLookup(const anilist::titleRef& title) {
	auto found = lookUpPaced(title);
	std::lock_guard<std::mutex> lock(mutex);
	if (not found.ok()) {
		// Not reached: a later change or check may try again.
		lookedUp.erase(canonical(title));
		return;
	}
	findings.push_back({title, found.value().facts});
}

std::optional<std::string> updater::shared::imageUrlOf(const anilist::titleRef& title) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto known = imageUrls.find(canonical(title));
		if (known != imageUrls.end())
			return known->second;
	}
	auto found = lookUpPaced(title);
	if (not found.ok())
		return std::nullopt;
	return found.value().imageUrl;
}

void updater::shared::runCover(const job& next) {
	// The image itself is fetched once; each size is converted from it.
	std::optional<std::string> image;
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto known = images.find(canonical(next.title));
		if (known != images.end())
			image = known->second;
	}
	coverState outcome = coverState::kReady;
	if (not image) {
		auto url = imageUrlOf(next.title);
		if (not url)
			outcome = coverState::kUnreachable;
		else if (url->empty())
			outcome = coverState::kNone;
		else {
			// The image server is not the API, so no pause is needed.
			image = fetch(*url);
			if (not image)
				outcome = coverState::kUnreachable;
		}
	}
	std::optional<std::string> sixel;
	if (image) {
		sixel = toSixel(*image, next.widthPx, next.heightPx);
		if (not sixel)
			outcome = coverState::kUnreadable;
	}

	// Record what happened, under the key the lookup may just have settled.
	std::lock_guard<std::mutex> lock(mutex);
	titleKey title = canonical(next.title);
	if (image)
		images[title] = *image;
	if (sixel) {
		sixels[{title.first, title.second, next.widthPx, next.heightPx}] = *sixel;
		return;
	}
	failed[title] = outcome;
	if (outcome == coverState::kUnreachable)
		retryAt[title] = std::chrono::steady_clock::now() + retryAfter;
}

void updater::shared::runLink(const job& next) {
	auto pacedPost = [this](const std::string& url, const std::string& json) {
		return waitTurn() ? post(url, json) : std::nullopt;
	};
	auto found = anilist::linkIds(pacedPost, next.title.type, next.known, next.ids);
	if (not found.ok())
		return;
	std::lock_guard<std::mutex> lock(mutex);
	links.push_back({next.title.type, found.value()});
}

std::vector<missingIds> findMissingIds(const catalogue& lists) {
	std::vector<missingIds> missing;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		missingIds byMal{type, provider::kMyAnimeList, {}};
		missingIds byAniList{type, provider::kAniList, {}};
		for (const auto& e : lists.getEntries(type)) {
			if (e.anilistId == 0 and e.malId > 0)
				byMal.ids.push_back(e.malId);
			if (e.malId == 0 and e.anilistId > 0)
				byAniList.ids.push_back(e.anilistId);
		}
		for (auto* each : {&byMal, &byAniList})
			if (not each->ids.empty())
				missing.push_back(std::move(*each));
	}
	return missing;
}

int fillIds(catalogue& lists, mediaType type, const std::vector<anilist::idPair>& found) {
	std::vector<std::pair<int, int>> pairs;
	for (const auto& each : found)
		pairs.emplace_back(each.anilistId, each.malId);
	return lists.fillIds(type, pairs);
}

void checkFollowed(const catalogue& lists, updater& web, const isoDate& today) {
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		for (const auto& e : lists.getEntries(type)) {
			bool following = e.status == watchStatus::kWatching or e.status == watchStatus::kOnHold;
			if (following and e.checkedAt != today)
				web.lookUp(anilist::refOf(e));
		}
	}
}

}  // namespace catalist
