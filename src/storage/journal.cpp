#include "storage/journal.hpp"

#include <fmt/format.h>

#include <algorithm>

namespace catalist::journal {

namespace {

std::string dateText(const std::optional<isoDate>& day) {
	return day ? day->toString() : std::string("-");
}

// One field the person sets: its name, its value as text, and how to copy
// it from one entry to another.
struct column {
	const char* name;
	std::string (*text)(const entry& of);
	void (*copy)(entry& into, const entry& from);
};

const std::vector<column>& columns() {
	static const std::vector<column> all = {
	    {"progress", [](const entry& e) { return std::to_string(e.progress); },
	     [](entry& into, const entry& from) { into.progress = from.progress; }},
	    {"status", [](const entry& e) { return statusKey(e.status); },
	     [](entry& into, const entry& from) { into.status = from.status; }},
	    {"score", [](const entry& e) { return std::to_string(e.score); },
	     [](entry& into, const entry& from) { into.score = from.score; }},
	    {"total", [](const entry& e) { return std::to_string(e.totalUnits); },
	     [](entry& into, const entry& from) { into.totalUnits = from.totalUnits; }},
	    {"volumes", [](const entry& e) { return std::to_string(e.volumes); },
	     [](entry& into, const entry& from) { into.volumes = from.volumes; }},
	    {"rewatched", [](const entry& e) { return std::to_string(e.rewatched); },
	     [](entry& into, const entry& from) { into.rewatched = from.rewatched; }},
	    {"started", [](const entry& e) { return dateText(e.startDate); },
	     [](entry& into, const entry& from) { into.startDate = from.startDate; }},
	    {"finished", [](const entry& e) { return dateText(e.finishDate); },
	     [](entry& into, const entry& from) { into.finishDate = from.finishDate; }},
	    {"title", [](const entry& e) { return e.title; },
	     [](entry& into, const entry& from) { into.title = from.title; }},
	};
	return all;
}

bool sameFields(const entry& a, const entry& b) {
	const auto& all = columns();
	return std::all_of(all.begin(), all.end(),
	                   [&](const column& each) { return each.text(a) == each.text(b); });
}

const entry* findById(const std::vector<entry>& list, int id) {
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const entry& e) { return e.id == id; });
	return found == list.end() ? nullptr : &*found;
}

// The entry's title and ref, as refs::of writes it: "Odd Taxi (a:2)".
std::string nameOf(const entry& named) {
	return fmt::format("{} ({}:{})", named.title, named.type == mediaType::kAnime ? 'a' : 'm',
	                   named.id);
}

using outcome = result<std::optional<entry>>;

// The entry comes back: its id and its site ids must be free.
outcome comeBack(const std::vector<entry>& list, const entry& back) {
	for (const auto& other : list) {
		if (other.id == back.id)
			return outcome::failure(fmt::format("{} cannot come back: its ref is now that of {}",
			                                    nameOf(back), other.title));
		bool same = (back.anilistId > 0 and other.anilistId == back.anilistId) or
		            (back.malId > 0 and other.malId == back.malId) or
		            (back.kitsuId > 0 and other.kitsuId == back.kitsuId);
		if (same)
			return outcome::failure(
			    fmt::format("{} is in your lists again, as {}", back.title, nameOf(other)));
	}
	return outcome::success(back);
}

// The first field the change set that no longer holds what it was set to,
// as "progress is now 7, not 6"; empty when all of them do.
std::string changedSince(const entry& now, const entry& from, const std::optional<entry>& to) {
	for (const auto& each : columns()) {
		bool wasSet = not to or each.text(from) != each.text(*to);
		if (wasSet and each.text(now) != each.text(from))
			return fmt::format("{} is now {}, not {}", each.name, each.text(now), each.text(from));
	}
	return "";
}

}  // namespace

std::vector<change> diff(const std::vector<entry>& saved, const std::vector<entry>& current) {
	std::vector<change> changes;
	for (const auto& now : current) {
		const entry* was = findById(saved, now.id);
		if (not was)
			changes.push_back({now.type, now.id, std::nullopt, now});
		else if (not sameFields(*was, now))
			changes.push_back({now.type, now.id, *was, now});
	}
	for (const auto& was : saved)
		if (not findById(current, was.id))
			changes.push_back({was.type, was.id, was, std::nullopt});
	return changes;
}

std::vector<fieldChange> changedFields(const change& made) {
	if (not made.before and not made.after)
		return {};
	if (not made.before)
		return {{"added", "-", statusKey(made.after->status)}};
	if (not made.after)
		return {{"removed", statusKey(made.before->status), "-"}};
	std::vector<fieldChange> fields;
	for (const auto& each : columns()) {
		std::string before = each.text(*made.before);
		std::string after = each.text(*made.after);
		if (before != after)
			fields.push_back({each.name, before, after});
	}
	return fields;
}

result<std::optional<entry>> transition(const std::vector<entry>& list,
                                        const std::optional<entry>& from,
                                        const std::optional<entry>& to, bool force) {
	if (not from)
		return to ? comeBack(list, *to) : outcome::success(std::nullopt);

	const entry* now = findById(list, from->id);
	if (not now) {
		// Gone already: nothing to remove, and nothing to set fields on.
		if (force and not to)
			return outcome::success(std::nullopt);
		return outcome::failure(fmt::format("{} is no longer in your lists", nameOf(*from)));
	}
	std::string moved = force ? "" : changedSince(*now, *from, to);
	if (not moved.empty())
		return outcome::failure(fmt::format("{} has changed since: {}", nameOf(*now), moved));
	if (not to)
		return outcome::success(std::nullopt);

	entry next = *now;
	for (const auto& each : columns())
		if (each.text(*from) != each.text(*to))
			each.copy(next, *to);
	next.updatedAt = to->updatedAt;
	return outcome::success(next);
}

}  // namespace catalist::journal
