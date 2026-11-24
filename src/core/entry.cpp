#include "core/entry.hpp"

#include <algorithm>
#include <format>

#include "core/text.hpp"

namespace catalist {

std::string StatusToString(WatchStatus status, MediaType type) {
	switch (status) {
		case WatchStatus::kWatching:
			return (type == MediaType::kManga) ? "Reading" : "Watching";
		case WatchStatus::kCompleted:
			return "Completed";
		case WatchStatus::kOnHold:
			return "On-Hold";
		case WatchStatus::kDropped:
			return "Dropped";
		case WatchStatus::kPlanToWatch:
			return (type == MediaType::kManga) ? "Plan to Read" : "Plan to Watch";
		case WatchStatus::kRewatching:
			return (type == MediaType::kManga) ? "Rereading" : "Rewatching";
	}
	return "Unknown";
}

std::string StatusKey(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return "watching";
		case WatchStatus::kCompleted:
			return "completed";
		case WatchStatus::kOnHold:
			return "on-hold";
		case WatchStatus::kDropped:
			return "dropped";
		case WatchStatus::kPlanToWatch:
			return "planned";
		case WatchStatus::kRewatching:
			return "rewatching";
	}
	return "planned";
}

std::optional<WatchStatus> ParseStatus(const std::string& str) {
	std::string lower = utf8::Lowercase(str);

	if (lower.find("plan") != std::string::npos or lower == "5") {
		return WatchStatus::kPlanToWatch;
	} else if (lower.find("rewatch") != std::string::npos or
	           lower.find("reread") != std::string::npos or
	           lower.find("repeat") != std::string::npos) {
		return WatchStatus::kRewatching;
	} else if (lower.find("watch") != std::string::npos or
	           lower.find("read") != std::string::npos or lower == "1") {
		return WatchStatus::kWatching;
	} else if (lower.find("complete") != std::string::npos or lower == "2") {
		return WatchStatus::kCompleted;
	} else if (lower.find("hold") != std::string::npos or lower == "3") {
		return WatchStatus::kOnHold;
	} else if (lower.find("drop") != std::string::npos or lower == "4") {
		return WatchStatus::kDropped;
	}
	return std::nullopt;
}

const std::string& DisplayName(const Entry& shown) {
	return shown.english_title.empty() ? shown.title : shown.english_title;
}

std::string PageOf(const Entry& shown) {
	if (shown.anilist_id <= 0)
		return "";
	return std::format("https://anilist.co/{}/{}", TypeKey(shown.type), shown.anilist_id);
}

std::string TypeToString(MediaType type) {
	switch (type) {
		case MediaType::kAnime:
			return "Anime";
		case MediaType::kManga:
			return "Manga";
	}
	return "Anime";
}

std::string TypeKey(MediaType type) {
	return type == MediaType::kAnime ? "anime" : "manga";
}

std::string FormatDate(const std::optional<IsoDate>& date) {
	return date ? date->ToString() : "";
}

std::optional<IsoDate> ListDate(const Entry& shown) {
	if (shown.status == WatchStatus::kCompleted and shown.finish_date)
		return shown.finish_date;
	return shown.start_date ? shown.start_date : shown.updated_at;
}

}  // namespace catalist

namespace catalist {

namespace {

bool HasKnownTotal(const Entry& target) {
	return target.total_units > 0;
}

bool IsPaused(WatchStatus status) {
	return status == WatchStatus::kPlanToWatch or status == WatchStatus::kOnHold or
	       status == WatchStatus::kDropped;
}

Entry ApplyStatusDates(Entry target, WatchStatus previous, const IsoDate& today) {
	switch (target.status) {
		case WatchStatus::kWatching:
			if (not target.start_date)
				target.start_date = today;
			target.finish_date.reset();
			break;
		case WatchStatus::kCompleted:
			if (not target.start_date)
				target.start_date = today;
			if (previous != WatchStatus::kRewatching or not target.finish_date)
				target.finish_date = today;
			if (HasKnownTotal(target))
				target.progress = std::max(target.progress, target.total_units);
			if (previous == WatchStatus::kRewatching)
				++target.rewatched;
			break;
		case WatchStatus::kOnHold:
		case WatchStatus::kDropped:
		case WatchStatus::kPlanToWatch:
			target.finish_date.reset();
			break;
		case WatchStatus::kRewatching:
			if (previous == WatchStatus::kCompleted)
				target.progress = 0;
			if (not target.start_date)
				target.start_date = today;
			break;
	}
	return target;
}

Entry WithProgress(Entry target, int progress, const IsoDate& today) {
	target.progress = progress;
	if (AtTotal(target)) {
		target.progress = target.total_units;
		if (target.status == WatchStatus::kRewatching)
			++target.rewatched;
		target.status = WatchStatus::kCompleted;
	}
	target.updated_at = today;
	return target;
}

Entry StepForward(Entry target, int new_progress, const IsoDate& today) {
	bool again = target.status == WatchStatus::kRewatching;
	if (not target.start_date)
		target.start_date = today;
	if (IsPaused(target.status)) {
		target.status = WatchStatus::kWatching;
		target.finish_date.reset();
	}
	target = WithProgress(target, new_progress, today);
	if (AtTotal(target) and (not again or not target.finish_date))
		target.finish_date = today;
	return target;
}

Entry StepBackward(Entry target, int new_progress, const IsoDate& today) {
	if (target.status == WatchStatus::kCompleted) {
		target.status = WatchStatus::kWatching;
		target.finish_date.reset();
	}
	target.progress = new_progress;
	target.updated_at = today;
	return target;
}

}  // namespace

bool AtTotal(const Entry& target) {
	return HasKnownTotal(target) and target.progress >= target.total_units;
}

std::optional<std::string> StatusRefusal(const Entry& target, WatchStatus status) {
	if (status == WatchStatus::kWatching and target.status == WatchStatus::kCompleted)
		return std::format("{} is completed; choose Rewatching to start it again",
		                   DisplayName(target));
	bool first_time =
	    target.status != WatchStatus::kCompleted and target.status != WatchStatus::kRewatching;
	if (status == WatchStatus::kRewatching and first_time)
		return std::format("{} is not completed; choose Watching to carry on",
		                   DisplayName(target));
	return std::nullopt;
}

Entry StepProgress(Entry target, int delta, const IsoDate& today) {
	int new_progress = std::max(0, target.progress + delta);
	if (new_progress == target.progress or (delta > 0 and AtTotal(target)))
		return target;
	return delta > 0 ? StepForward(target, new_progress, today)
	                 : StepBackward(target, new_progress, today);
}

Entry SetStatus(Entry target, WatchStatus status, const IsoDate& today) {
	if (status == target.status)
		return target;
	WatchStatus previous = target.status;
	if (previous == WatchStatus::kRewatching) {
		if (status == WatchStatus::kWatching)
			return target;
		if (IsPaused(status)) {
			target.status = WatchStatus::kCompleted;
			if (HasKnownTotal(target))
				target.progress = target.total_units;
			target.updated_at = today;
			return target;
		}
	}
	target.status = status;
	target = ApplyStatusDates(target, previous, today);
	target.updated_at = today;
	return target;
}

Entry SetScore(Entry target, int score, const IsoDate& today) {
	score = std::clamp(score, 0, 10);
	if (score == target.score)
		return target;
	target.score = score;
	target.updated_at = today;
	return target;
}

int CountOf(const Entry& target, CountField field) {
	switch (field) {
		case CountField::kProgress:
			return target.progress;
		case CountField::kTotalUnits:
			return target.total_units;
		case CountField::kVolumes:
			return target.volumes;
		case CountField::kRewatched:
			return target.rewatched;
	}
	return 0;
}

Entry SetCount(Entry target, CountField field, int value, const IsoDate& today) {
	value = std::max(0, value);
	if (field == CountField::kProgress)
		return StepProgress(target, value - target.progress, today);
	if (field == CountField::kVolumes and target.total_volumes > 0)
		value = std::min(value, target.total_volumes);
	if (value == CountOf(target, field))
		return target;

	if (field == CountField::kTotalUnits) {
		target.total_units = value;
		if (value > 0)
			target.progress = std::min(target.progress, value);
	} else if (field == CountField::kVolumes) {
		target.volumes = value;
	} else {
		target.rewatched = value;
	}
	target.updated_at = today;
	return target;
}

Entry SetTitle(Entry target, const std::string& title, const IsoDate& today) {
	if (title == DisplayName(target))
		return target;
	target.english_title = title;
	target.updated_at = today;
	return target;
}

Entry SetStartDate(Entry target, std::optional<IsoDate> day, const IsoDate& today) {
	if (day == target.start_date)
		return target;
	target.start_date = day;
	target.updated_at = today;
	return target;
}

Entry SetFinishDate(Entry target, std::optional<IsoDate> day, const IsoDate& today) {
	if (day == target.finish_date)
		return target;
	target.finish_date = day;
	target.updated_at = today;
	return target;
}

Entry MergeImported(Entry existing, const Entry& incoming) {
	if (incoming.total_units > 0)
		existing.total_units = incoming.total_units;
	if (incoming.total_volumes > 0)
		existing.total_volumes = incoming.total_volumes;
	if (incoming.mal_id > 0)
		existing.mal_id = incoming.mal_id;
	if (incoming.anilist_id > 0)
		existing.anilist_id = incoming.anilist_id;
	if (existing.score == 0 and incoming.score > 0)
		existing.score = incoming.score;
	if (existing.progress == 0 and incoming.progress > 0)
		existing.progress = incoming.progress;
	if (not existing.start_date and incoming.start_date)
		existing.start_date = incoming.start_date;
	bool can_finish =
	    existing.status == WatchStatus::kCompleted or existing.status == WatchStatus::kRewatching;
	if (can_finish and not existing.finish_date and incoming.finish_date)
		existing.finish_date = incoming.finish_date;
	return existing;
}

Entry TakeImported(Entry existing, const Entry& incoming) {
	existing = MergeImported(existing, incoming);
	existing.status = incoming.status;
	for (int Entry::* count :
	     {&Entry::score, &Entry::progress, &Entry::volumes, &Entry::rewatched})
		if (incoming.*count > 0)
			existing.*count = incoming.*count;
	if (incoming.start_date)
		existing.start_date = incoming.start_date;
	if (incoming.finish_date)
		existing.finish_date = incoming.finish_date;
	if (incoming.status != WatchStatus::kCompleted and incoming.status != WatchStatus::kRewatching)
		existing.finish_date.reset();
	return existing;
}

}  // namespace catalist
