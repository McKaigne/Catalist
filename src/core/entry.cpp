#include "core/entry.hpp"

#include <algorithm>
#include <format>

namespace catalist {

std::string statusToString(watchStatus status, mediaType type) {
	switch (status) {
		case watchStatus::kWatching:
			return (type == mediaType::kManga) ? "Reading" : "Watching";
		case watchStatus::kCompleted:
			return "Completed";
		case watchStatus::kOnHold:
			return "On-Hold";
		case watchStatus::kDropped:
			return "Dropped";
		case watchStatus::kPlanToWatch:
			return (type == mediaType::kManga) ? "Plan to Read" : "Plan to Watch";
		case watchStatus::kRewatching:
			return (type == mediaType::kManga) ? "Rereading" : "Rewatching";
	}
	return "Unknown";
}

unsigned mergeFields(unsigned first, unsigned second) {
	auto upOnly = [](unsigned fields) {
		return not(fields & siteField::kProgress) or (fields & siteField::kRaiseOnly);
	};
	unsigned merged = (first | second) & ~siteField::kRaiseOnly;
	if ((merged & siteField::kProgress) and upOnly(first) and upOnly(second))
		merged |= siteField::kRaiseOnly;
	return merged;
}

std::string statusKey(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return "watching";
		case watchStatus::kCompleted:
			return "completed";
		case watchStatus::kOnHold:
			return "on-hold";
		case watchStatus::kDropped:
			return "dropped";
		case watchStatus::kPlanToWatch:
			return "planned";
		case watchStatus::kRewatching:
			return "rewatching";
	}
	return "planned";
}

std::optional<watchStatus> parseStatus(const std::string& str) {
	std::string lower = str;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

	if (lower.find("plan") != std::string::npos or lower == "5") {
		return watchStatus::kPlanToWatch;
	} else if (lower.find("rewatch") != std::string::npos or
	           lower.find("reread") != std::string::npos or
	           lower.find("repeat") != std::string::npos) {
		return watchStatus::kRewatching;
	} else if (lower.find("watch") != std::string::npos or
	           lower.find("read") != std::string::npos or lower == "1") {
		return watchStatus::kWatching;
	} else if (lower.find("complete") != std::string::npos or lower == "2") {
		return watchStatus::kCompleted;
	} else if (lower.find("hold") != std::string::npos or lower == "3") {
		return watchStatus::kOnHold;
	} else if (lower.find("drop") != std::string::npos or lower == "4") {
		return watchStatus::kDropped;
	}
	return std::nullopt;
}

std::string providerName(provider site) {
	switch (site) {
		case provider::kAniList:
			return "AniList";
		case provider::kMyAnimeList:
			return "MyAnimeList";
		case provider::kKitsu:
			return "Kitsu";
	}
	return "AniList";
}

std::string providerKey(provider site) {
	switch (site) {
		case provider::kAniList:
			return "anilist";
		case provider::kMyAnimeList:
			return "mal";
		case provider::kKitsu:
			return "kitsu";
	}
	return "anilist";
}

std::optional<provider> parseProvider(const std::string& name) {
	std::string lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if (lower == "anilist" or lower == "al")
		return provider::kAniList;
	if (lower == "myanimelist" or lower == "mal")
		return provider::kMyAnimeList;
	if (lower == "kitsu")
		return provider::kKitsu;
	return std::nullopt;
}

int idOn(const entry& shown, provider site) {
	switch (site) {
		case provider::kAniList:
			return shown.anilistId;
		case provider::kMyAnimeList:
			return shown.malId;
		case provider::kKitsu:
			return shown.kitsuId;
	}
	return 0;
}

std::string pageOn(const entry& shown, provider site) {
	int id = idOn(shown, site);
	if (id <= 0)
		return "";
	std::string kind = shown.type == mediaType::kAnime ? "anime" : "manga";
	switch (site) {
		case provider::kAniList:
			return std::format("https://anilist.co/{}/{}", kind, id);
		case provider::kMyAnimeList:
			return std::format("https://myanimelist.net/{}/{}", kind, id);
		case provider::kKitsu:
			return std::format("https://kitsu.app/{}/{}", kind, id);
	}
	return "";
}

std::string typeToString(mediaType type) {
	switch (type) {
		case mediaType::kAnime:
			return "Anime";
		case mediaType::kManga:
			return "Manga";
	}
	return "Anime";
}

std::string typeKey(mediaType type) {
	return type == mediaType::kAnime ? "anime" : "manga";
}

std::string formatDate(const std::optional<isoDate>& date) {
	return date ? date->toString() : "";
}

std::optional<isoDate> listDate(const entry& shown) {
	if (shown.status == watchStatus::kCompleted and shown.finishDate)
		return shown.finishDate;
	return shown.startDate ? shown.startDate : shown.updatedAt;
}

int newReleases(const entry& shown) {
	bool following = shown.status == watchStatus::kWatching or shown.status == watchStatus::kOnHold;
	return following ? std::max(0, shown.released - shown.progress) : 0;
}

}  // namespace catalist

namespace catalist {

namespace {

bool hasKnownTotal(const entry& target) {
	return target.totalUnits > 0;
}

bool isFinished(const entry& target) {
	return hasKnownTotal(target) and target.progress >= target.totalUnits;
}

bool isPaused(watchStatus status) {
	return status == watchStatus::kPlanToWatch or status == watchStatus::kOnHold or
	       status == watchStatus::kDropped;
}

entry applyStatusDates(entry target, watchStatus previous, const isoDate& today) {
	switch (target.status) {
		case watchStatus::kWatching:
			if (not target.startDate)
				target.startDate = today;
			if (previous == watchStatus::kCompleted)
				target.finishDate.reset();
			break;
		case watchStatus::kCompleted:
			if (not target.startDate)
				target.startDate = today;
			target.finishDate = today;
			if (hasKnownTotal(target))
				target.progress = std::max(target.progress, target.totalUnits);
			break;
		case watchStatus::kOnHold:
		case watchStatus::kDropped:
			if (not target.finishDate)
				target.finishDate = today;
			break;
		case watchStatus::kPlanToWatch:
			target.finishDate.reset();
			break;
		case watchStatus::kRewatching:
			// Again from the start; the dates stay those of the first time.
			if (previous == watchStatus::kCompleted)
				target.progress = 0;
			if (not target.startDate)
				target.startDate = today;
			break;
	}
	return target;
}

entry withProgress(entry target, int progress, const isoDate& today) {
	target.progress = progress;
	if (isFinished(target)) {
		target.progress = target.totalUnits;
		if (target.status == watchStatus::kRewatching)
			++target.rewatched;
		target.status = watchStatus::kCompleted;
	}
	target.updatedAt = today;
	return target;
}

entry stepForward(entry target, int newProgress, const isoDate& today) {
	if (not target.startDate)
		target.startDate = today;
	if (isPaused(target.status))
		target.status = watchStatus::kWatching;
	target = withProgress(target, newProgress, today);
	if (isFinished(target))
		target.finishDate = today;
	return target;
}

entry stepBackward(entry target, int newProgress, const isoDate& today) {
	bool reopens = isFinished(target) and newProgress < target.totalUnits;
	if (reopens) {
		target.status = watchStatus::kWatching;
		target.finishDate.reset();
	}
	target.progress = newProgress;
	target.updatedAt = today;
	return target;
}

}  // namespace

entry stepProgress(entry target, int delta, const isoDate& today) {
	int newProgress = std::max(0, target.progress + delta);
	if (newProgress == target.progress or (delta > 0 and isFinished(target)))
		return target;
	return delta > 0 ? stepForward(target, newProgress, today)
	                 : stepBackward(target, newProgress, today);
}

entry setStatus(entry target, watchStatus status, const isoDate& today) {
	if (status == target.status)
		return target;
	watchStatus previous = target.status;
	target.status = status;
	target = applyStatusDates(target, previous, today);
	target.updatedAt = today;
	return target;
}

entry setScore(entry target, int score, const isoDate& today) {
	score = std::clamp(score, 0, 10);
	if (score == target.score)
		return target;
	target.score = score;
	target.updatedAt = today;
	return target;
}

int countOf(const entry& target, countField field) {
	switch (field) {
		case countField::kProgress:
			return target.progress;
		case countField::kTotalUnits:
			return target.totalUnits;
		case countField::kVolumes:
			return target.volumes;
		case countField::kRewatched:
			return target.rewatched;
	}
	return 0;
}

entry setCount(entry target, countField field, int value, const isoDate& today) {
	value = std::max(0, value);
	if (field == countField::kProgress)
		return stepProgress(target, value - target.progress, today);
	if (field == countField::kVolumes and target.totalVolumes > 0)
		value = std::min(value, target.totalVolumes);
	if (value == countOf(target, field))
		return target;

	if (field == countField::kTotalUnits) {
		target.totalUnits = value;
		if (value > 0)
			target.progress = std::min(target.progress, value);
	} else if (field == countField::kVolumes) {
		target.volumes = value;
	} else {
		target.rewatched = value;
	}
	target.updatedAt = today;
	return target;
}

entry setTitle(entry target, const std::string& title, const isoDate& today) {
	if (title == target.title)
		return target;
	target.title = title;
	target.updatedAt = today;
	return target;
}

entry setStartDate(entry target, std::optional<isoDate> day, const isoDate& today) {
	if (day == target.startDate)
		return target;
	target.startDate = day;
	target.updatedAt = today;
	return target;
}

entry setFinishDate(entry target, std::optional<isoDate> day, const isoDate& today) {
	if (day == target.finishDate)
		return target;
	target.finishDate = day;
	target.updatedAt = today;
	return target;
}

entry mergeImported(entry existing, const entry& incoming) {
	if (incoming.totalUnits > 0)
		existing.totalUnits = incoming.totalUnits;
	if (incoming.totalVolumes > 0)
		existing.totalVolumes = incoming.totalVolumes;
	if (incoming.malId > 0)
		existing.malId = incoming.malId;
	if (incoming.anilistId > 0)
		existing.anilistId = incoming.anilistId;
	if (incoming.kitsuId > 0)
		existing.kitsuId = incoming.kitsuId;
	if (existing.score == 0 and incoming.score > 0)
		existing.score = incoming.score;
	if (existing.progress == 0 and incoming.progress > 0)
		existing.progress = incoming.progress;
	if (not existing.startDate and incoming.startDate)
		existing.startDate = incoming.startDate;
	if (not existing.finishDate and incoming.finishDate)
		existing.finishDate = incoming.finishDate;
	return existing;
}

entry applyWebFacts(entry target, const webFacts& facts, const isoDate& today) {
	if (target.anilistId == 0)
		target.anilistId = facts.anilistId;
	if (target.malId == 0)
		target.malId = facts.malId;
	if (target.kitsuId == 0)
		target.kitsuId = facts.kitsuId;
	if (facts.totalUnits > 0)
		target.totalUnits = facts.totalUnits;
	if (facts.totalVolumes > 0)
		target.totalVolumes = facts.totalVolumes;
	if (facts.released > 0)
		target.released = facts.released;
	target.checkedAt = today;
	return target;
}

}  // namespace catalist
