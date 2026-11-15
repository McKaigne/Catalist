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

using titleKey = std::pair<mediaType, int>;
using coverKey = std::tuple<mediaType, int, int, int>;

struct job {
	bool isCover = false;
	coverKey key;
};

}  // namespace

struct updater::shared {
	shared(jikan::fetcher fetch, anilist::poster post, std::function<isoDate()> today,
	       sixelMaker toSixel, std::chrono::milliseconds pause, std::function<void()> wake,
	       std::chrono::milliseconds retryAfter)
	    : fetch(std::move(fetch)),
	      post(std::move(post)),
	      today(std::move(today)),
	      toSixel(std::move(toSixel)),
	      pause(pause),
	      wake(std::move(wake)),
	      retryAfter(retryAfter) {}

	void work();
	void runLookup(const titleKey& title);
	void runCover(const coverKey& key);
	// Waits until the pause since the last request is over; false when the
	// updater is stopping instead.
	bool waitTurn();
	// What AniList knows about the title, or else Jikan; fails when neither
	// could be reached or knows it.
	result<jikan::details> lookUpAnywhere(const titleKey& title);
	// The cover's address: known, looked up, absent because there is none
	// (an empty string), or nothing when no source could be reached.
	std::optional<std::string> imageUrlOf(const titleKey& title);

	const jikan::fetcher fetch;
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
	std::set<titleKey> lookedUp;
	std::vector<finding> findings;
	std::map<titleKey, std::string> imageUrls;
	std::map<titleKey, std::string> images;
	// Why a title has no cover, and when an unreachable one may be retried.
	std::map<titleKey, coverState> failed;
	std::map<titleKey, std::chrono::steady_clock::time_point> retryAt;
	std::set<coverKey> asked;
	std::map<coverKey, std::string> sixels;
};

updater::updater(jikan::fetcher fetch, anilist::poster post, std::function<isoDate()> today,
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

void updater::lookUp(mediaType type, int malId) {
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		if (malId <= 0 or not shared_->lookedUp.insert({type, malId}).second)
			return;
		shared_->jobs.push_back({false, {type, malId, 0, 0}});
	}
	shared_->changed.notify_all();
}

std::vector<updater::finding> updater::takeFindings() {
	std::lock_guard<std::mutex> lock(shared_->mutex);
	return std::exchange(shared_->findings, {});
}

updater::cover updater::coverFor(mediaType type, int malId, int widthPx, int heightPx) {
	coverKey key{type, malId, widthPx, heightPx};
	titleKey title{type, malId};
	{
		std::lock_guard<std::mutex> lock(shared_->mutex);
		auto ready = shared_->sixels.find(key);
		if (ready != shared_->sixels.end())
			return {coverState::kReady, ready->second};
		if (malId <= 0)
			return {coverState::kNone, ""};

		// A failure stands, except that an unreachable cover is asked for
		// again once its wait is over.
		auto failure = shared_->failed.find(title);
		if (failure != shared_->failed.end()) {
			bool retry = failure->second == coverState::kUnreachable and
			             std::chrono::steady_clock::now() >= shared_->retryAt[title];
			if (not retry)
				return {failure->second, ""};
			shared_->failed.erase(failure);
			shared_->asked.erase(key);
		}
		if (not shared_->asked.insert(key).second)
			return {coverState::kLoading, ""};
		// The person is looking at this one, so it goes before any lookups.
		shared_->jobs.push_front({true, key});
	}
	shared_->changed.notify_all();
	return {coverState::kLoading, ""};
}

void updater::waitUntilIdle() {
	std::unique_lock<std::mutex> lock(shared_->mutex);
	shared_->changed.wait(lock, [this] { return shared_->jobs.empty() and not shared_->working; });
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
		if (next.isCover)
			runCover(next.key);
		else
			runLookup({std::get<0>(next.key), std::get<1>(next.key)});

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

result<jikan::details> updater::shared::lookUpAnywhere(const titleKey& title) {
	auto [type, malId] = title;
	// AniList first: it is the more reliable of the two and counts the
	// episodes of an airing anime directly.
	auto pacedPost = [this](const std::string& url, const std::string& json) {
		return waitTurn() ? post(url, json) : std::nullopt;
	};
	auto found = anilist::lookUp(pacedPost, type, malId);
	if (found.ok())
		return found;
	auto pacedFetch = [this](const std::string& url) {
		return waitTurn() ? fetch(url) : std::nullopt;
	};
	return jikan::lookUp(pacedFetch, type, malId, today());
}

void updater::shared::runLookup(const titleKey& title) {
	auto found = lookUpAnywhere(title);
	std::lock_guard<std::mutex> lock(mutex);
	if (not found.ok()) {
		// Not reached: a later change or check may try again.
		lookedUp.erase(title);
		return;
	}
	imageUrls[title] = found.value().imageUrl;
	findings.push_back({title.first, title.second, found.value().facts});
}

std::optional<std::string> updater::shared::imageUrlOf(const titleKey& title) {
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto known = imageUrls.find(title);
		if (known != imageUrls.end())
			return known->second;
	}
	auto found = lookUpAnywhere(title);
	if (not found.ok())
		return std::nullopt;
	std::lock_guard<std::mutex> lock(mutex);
	imageUrls[title] = found.value().imageUrl;
	return found.value().imageUrl;
}

void updater::shared::runCover(const coverKey& key) {
	auto [type, malId, widthPx, heightPx] = key;
	titleKey title{type, malId};

	// The image itself is fetched once; each size is converted from it.
	std::optional<std::string> image;
	{
		std::lock_guard<std::mutex> lock(mutex);
		auto known = images.find(title);
		if (known != images.end())
			image = known->second;
	}
	coverState outcome = coverState::kReady;
	if (not image) {
		auto url = imageUrlOf(title);
		if (not url)
			outcome = coverState::kUnreachable;
		else if (url->empty())
			outcome = coverState::kNone;
		else {
			// The image server is not Jikan, so no pause is needed.
			image = fetch(*url);
			if (not image)
				outcome = coverState::kUnreachable;
		}
	}
	std::optional<std::string> sixel;
	if (image) {
		sixel = toSixel(*image, widthPx, heightPx);
		if (not sixel)
			outcome = coverState::kUnreadable;
	}

	// Record what happened.
	std::lock_guard<std::mutex> lock(mutex);
	if (image)
		images[title] = *image;
	if (sixel) {
		sixels[key] = *sixel;
		return;
	}
	failed[title] = outcome;
	if (outcome == coverState::kUnreachable)
		retryAt[title] = std::chrono::steady_clock::now() + retryAfter;
}

void checkFollowed(const catalogue& lists, updater& web, const isoDate& today) {
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		for (const auto& e : lists.getEntries(type)) {
			bool following = e.status == watchStatus::kWatching or e.status == watchStatus::kOnHold;
			if (following and e.malId > 0 and e.checkedAt != today)
				web.lookUp(type, e.malId);
		}
	}
}

}  // namespace catalist
