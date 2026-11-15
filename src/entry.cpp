#include "entry.hpp"

#include <algorithm>

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
	}
	return "Unknown";
}

std::optional<watchStatus> parseStatus(const std::string& str) {
	std::string lower = str;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

	if (lower.find("plan") != std::string::npos or lower == "5") {
		return watchStatus::kPlanToWatch;
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

std::string typeToString(mediaType type) {
	switch (type) {
		case mediaType::kAnime:
			return "Anime";
		case mediaType::kManga:
			return "Manga";
	}
	return "Anime";
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

watchStatus nextStatus(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return watchStatus::kCompleted;
		case watchStatus::kCompleted:
			return watchStatus::kOnHold;
		case watchStatus::kOnHold:
			return watchStatus::kDropped;
		case watchStatus::kDropped:
			return watchStatus::kPlanToWatch;
		case watchStatus::kPlanToWatch:
			return watchStatus::kWatching;
	}
	return status;
}

watchStatus previousStatus(watchStatus status) {
	switch (status) {
		case watchStatus::kWatching:
			return watchStatus::kPlanToWatch;
		case watchStatus::kCompleted:
			return watchStatus::kWatching;
		case watchStatus::kOnHold:
			return watchStatus::kCompleted;
		case watchStatus::kDropped:
			return watchStatus::kOnHold;
		case watchStatus::kPlanToWatch:
			return watchStatus::kDropped;
	}
	return status;
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
	}
	return target;
}

entry withProgress(entry target, int progress, const isoDate& today) {
	target.progress = progress;
	if (isFinished(target)) {
		target.progress = target.totalUnits;
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

entry cycleStatus(entry target, int direction, const isoDate& today) {
	watchStatus previous = target.status;
	target.status = direction > 0 ? nextStatus(previous) : previousStatus(previous);
	target = applyStatusDates(target, previous, today);
	target.updatedAt = today;
	return target;
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
	return adjustRating(target, score - target.score, today);
}

entry adjustRating(entry target, int delta, const isoDate& today) {
	int score = std::clamp(target.score + delta, 0, 10);
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

entry mergeImported(entry existing, const entry& incoming) {
	if (incoming.totalUnits > 0)
		existing.totalUnits = incoming.totalUnits;
	if (incoming.totalVolumes > 0)
		existing.totalVolumes = incoming.totalVolumes;
	if (incoming.malId > 0)
		existing.malId = incoming.malId;
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
