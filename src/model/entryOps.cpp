#include "catalist/model/entryOps.hpp"

#include <algorithm>

namespace catalist {

namespace {

bool hasKnownTotal(const entry& target) {
  return target.totalUnits > 0;
}

bool isFinished(const entry& target) {
  return hasKnownTotal(target) && target.progress >= target.totalUnits;
}

bool isPaused(watchStatus status) {
  return status == watchStatus::kPlanToWatch ||
         status == watchStatus::kOnHold || status == watchStatus::kDropped;
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

entry applyStatusDates(entry target, watchStatus previous,
                       const isoDate& today) {
  switch (target.status) {
    case watchStatus::kWatching:
      if (!target.startDate)
        target.startDate = today;
      if (previous == watchStatus::kCompleted)
        target.finishDate.reset();
      break;
    case watchStatus::kCompleted:
      if (!target.startDate)
        target.startDate = today;
      target.finishDate = today;
      if (hasKnownTotal(target))
        target.progress = std::max(target.progress, target.totalUnits);
      break;
    case watchStatus::kOnHold:
    case watchStatus::kDropped:
      if (!target.finishDate)
        target.finishDate = today;
      break;
    case watchStatus::kPlanToWatch:
      target.finishDate.reset();
      break;
  }
  return target;
}

entry stepForward(entry target, int newProgress, const isoDate& today) {
  if (!target.startDate)
    target.startDate = today;
  if (isPaused(target.status))
    target.status = watchStatus::kWatching;
  target = setProgress(target, newProgress, today);
  if (isFinished(target))
    target.finishDate = today;
  return target;
}

entry stepBackward(entry target, int newProgress) {
  bool reopens = isFinished(target) && newProgress < target.totalUnits;
  if (reopens) {
    target.status = watchStatus::kWatching;
    target.finishDate.reset();
  }
  target.progress = newProgress;
  return target;
}

}  // namespace

entry setProgress(entry target, int progress, const isoDate& today) {
  target.progress = std::max(0, progress);
  if (isFinished(target)) {
    target.progress = target.totalUnits;
    target.status = watchStatus::kCompleted;
  }
  target.updatedAt = today;
  return target;
}

entry stepProgress(entry target, int delta, const isoDate& today) {
  int newProgress = std::max(0, target.progress + delta);
  if (delta > 0)
    target = stepForward(target, newProgress, today);
  if (delta < 0)
    target = stepBackward(target, newProgress);
  target.updatedAt = today;
  return target;
}

entry cycleStatus(entry target, int direction, const isoDate& today) {
  watchStatus previous = target.status;
  target.status =
      direction > 0 ? nextStatus(previous) : previousStatus(previous);
  target = applyStatusDates(target, previous, today);
  target.updatedAt = today;
  return target;
}

entry adjustRating(entry target, int delta, const isoDate& today) {
  target.score = std::clamp(target.score + delta, 0, 10);
  target.updatedAt = today;
  return target;
}

entry mergeImported(entry existing, const entry& incoming) {
  if (incoming.totalUnits > 0)
    existing.totalUnits = incoming.totalUnits;
  if (incoming.totalVolumes > 0)
    existing.totalVolumes = incoming.totalVolumes;
  for (const auto& [provider, remoteId] : incoming.remoteIds)
    existing.setRemoteId(provider, remoteId);
  if (incoming.malId > 0 && existing.malId <= 0)
    existing.setRemoteId("mal", std::to_string(incoming.malId));
  if (incoming.anilistId > 0 && existing.anilistId <= 0)
    existing.setRemoteId("anilist", std::to_string(incoming.anilistId));
  if (existing.notes.empty() && !incoming.notes.empty())
    existing.notes = incoming.notes;
  if (existing.score == 0 && incoming.score > 0)
    existing.score = incoming.score;
  if (existing.progress == 0 && incoming.progress > 0)
    existing.progress = incoming.progress;
  if (!existing.startDate && incoming.startDate)
    existing.startDate = incoming.startDate;
  if (!existing.finishDate && incoming.finishDate)
    existing.finishDate = incoming.finishDate;
  return existing;
}

}  // namespace catalist
