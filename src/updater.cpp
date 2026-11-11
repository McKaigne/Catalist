#include "updater.hpp"

namespace catalist {

updater::updater(jikan::fetcher fetch, std::function<isoDate()> today,
                 sixelMaker toSixel, std::chrono::milliseconds pause,
                 std::function<void()> wake)
    : fetch_(std::move(fetch)),
      today_(std::move(today)),
      toSixel_(std::move(toSixel)),
      pause_(pause),
      wake_(std::move(wake)) {
  worker_ = std::thread(&updater::work, this);
}

updater::~updater() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  changed_.notify_all();
  worker_.join();
}

void updater::lookUp(mediaType type, int malId) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (malId <= 0 || !lookedUp_.insert({type, malId}).second)
      return;
    jobs_.push_back({false, {type, malId, 0, 0}});
  }
  changed_.notify_all();
}

std::vector<updater::finding> updater::takeFindings() {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::exchange(findings_, {});
}

updater::cover updater::coverFor(mediaType type, int malId, int widthPx,
                                 int heightPx) {
  coverKey key{type, malId, widthPx, heightPx};
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto ready = sixels_.find(key);
    if (ready != sixels_.end())
      return {coverState::kReady, ready->second};
    if (malId <= 0 || noImage_.count({type, malId}))
      return {coverState::kNone, ""};
    if (!asked_.insert(key).second)
      return {coverState::kLoading, ""};
    // The person is looking at this one, so it goes before any lookups.
    jobs_.push_front({true, key});
  }
  changed_.notify_all();
  return {coverState::kLoading, ""};
}

void updater::waitUntilIdle() {
  std::unique_lock<std::mutex> lock(mutex_);
  changed_.wait(lock, [this] { return jobs_.empty() && !working_; });
}

void updater::work() {
  while (true) {
    job next;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      changed_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
      if (stopping_)
        return;
      next = jobs_.front();
      jobs_.pop_front();
      working_ = true;
    }

    // The requests run without the lock, so the interface never waits.
    if (next.isCover)
      runCover(next.key);
    else
      runLookup({std::get<0>(next.key), std::get<1>(next.key)});

    {
      std::lock_guard<std::mutex> lock(mutex_);
      working_ = false;
    }
    changed_.notify_all();
    if (wake_)
      wake_();
  }
}

std::optional<std::string> updater::pacedFetch(const std::string& url) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait_until(lock, lastRequest_ + pause_,
                        [this] { return stopping_; });
    if (stopping_)
      return std::nullopt;
    lastRequest_ = std::chrono::steady_clock::now();
  }
  return fetch_(url);
}

void updater::runLookup(const titleKey& title) {
  auto paced = [this](const std::string& url) { return pacedFetch(url); };
  auto found = jikan::lookUp(paced, title.first, title.second, today_());
  if (!found.ok())
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!found.value().imageUrl.empty())
    imageUrls_[title] = found.value().imageUrl;
  findings_.push_back({title.first, title.second, found.value().facts});
}

std::optional<std::string> updater::imageUrlOf(const titleKey& title) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto known = imageUrls_.find(title);
    if (known != imageUrls_.end())
      return known->second;
  }
  auto body = pacedFetch(jikan::detailsUrl(title.first, title.second));
  auto found = body ? jikan::parseDetails(*body, title.first)
                    : result<jikan::details>::failure("unreachable");
  if (!found.ok() || found.value().imageUrl.empty())
    return std::nullopt;
  std::lock_guard<std::mutex> lock(mutex_);
  imageUrls_[title] = found.value().imageUrl;
  return found.value().imageUrl;
}

void updater::runCover(const coverKey& key) {
  auto [type, malId, widthPx, heightPx] = key;
  titleKey title{type, malId};

  // The image itself is fetched once; each size is converted from it.
  std::optional<std::string> image;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto known = images_.find(title);
    if (known != images_.end())
      image = known->second;
  }
  if (!image) {
    auto url = imageUrlOf(title);
    image = url ? pacedFetch(*url) : std::nullopt;
  }
  auto sixel = image ? toSixel_(*image, widthPx, heightPx) : std::nullopt;

  std::lock_guard<std::mutex> lock(mutex_);
  if (image)
    images_[title] = *image;
  if (sixel)
    sixels_[key] = *sixel;
  else
    noImage_.insert(title);
}

void checkFollowed(const catalogue& lists, updater& web, const isoDate& today) {
  for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
    for (const auto& e : lists.getEntries(type)) {
      bool following = e.status == watchStatus::kWatching ||
                       e.status == watchStatus::kOnHold;
      if (following && e.malId > 0 && e.checkedAt != today)
        web.lookUp(type, e.malId);
    }
  }
}

}  // namespace catalist
