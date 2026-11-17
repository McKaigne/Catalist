#include "storage/journal.hpp"

#include <algorithm>
#include <format>

namespace catalist::journal {

namespace {

std::string DateText(const std::optional<IsoDate>& day) {
	return day ? day->ToString() : std::string("-");
}

// One field the person sets: its name, its value as text, and how to copy
// it from one entry to another.
struct Column {
	const char* name;
	std::string (*text)(const Entry& of);
	void (*copy)(Entry& into, const Entry& from);
};

const std::vector<Column>& Columns() {
	static const std::vector<Column> kAll = {
	    {"progress", [](const Entry& e) { return std::to_string(e.progress); },
	     [](Entry& into, const Entry& from) { into.progress = from.progress; }},
	    {"status", [](const Entry& e) { return StatusKey(e.status); },
	     [](Entry& into, const Entry& from) { into.status = from.status; }},
	    {"score", [](const Entry& e) { return std::to_string(e.score); },
	     [](Entry& into, const Entry& from) { into.score = from.score; }},
	    {"total", [](const Entry& e) { return std::to_string(e.total_units); },
	     [](Entry& into, const Entry& from) { into.total_units = from.total_units; }},
	    {"volumes", [](const Entry& e) { return std::to_string(e.volumes); },
	     [](Entry& into, const Entry& from) { into.volumes = from.volumes; }},
	    {"rewatched", [](const Entry& e) { return std::to_string(e.rewatched); },
	     [](Entry& into, const Entry& from) { into.rewatched = from.rewatched; }},
	    {"started", [](const Entry& e) { return DateText(e.start_date); },
	     [](Entry& into, const Entry& from) { into.start_date = from.start_date; }},
	    {"finished", [](const Entry& e) { return DateText(e.finish_date); },
	     [](Entry& into, const Entry& from) { into.finish_date = from.finish_date; }},
	    {"title", [](const Entry& e) { return e.title; },
	     [](Entry& into, const Entry& from) { into.title = from.title; }},
	};
	return kAll;
}

bool SameFields(const Entry& a, const Entry& b) {
	const auto& all = Columns();
	return std::all_of(all.begin(), all.end(),
	                   [&](const Column& each) { return each.text(a) == each.text(b); });
}

const Entry* FindById(const std::vector<Entry>& list, int id) {
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
	return found == list.end() ? nullptr : &*found;
}

// The entry's title and ref, as refs::Of writes it: "Odd Taxi (a:2)".
std::string NameOf(const Entry& named) {
	return std::format("{} ({}:{})", named.title, named.type == MediaType::kAnime ? 'a' : 'm',
	                   named.id);
}

using Outcome = Result<std::optional<Entry>>;

// The entry comes back: its id and its site ids must be free.
Outcome ComeBack(const std::vector<Entry>& list, const Entry& back) {
	for (const auto& other : list) {
		if (other.id == back.id)
			return std::unexpected(std::format("{} cannot come back: its ref is now that of {}",
			                                   NameOf(back), other.title));
		bool same = (back.anilist_id > 0 and other.anilist_id == back.anilist_id) or
		            (back.mal_id > 0 and other.mal_id == back.mal_id) or
		            (back.kitsu_id > 0 and other.kitsu_id == back.kitsu_id);
		if (same)
			return std::unexpected(
			    std::format("{} is in your lists again, as {}", back.title, NameOf(other)));
	}
	return back;
}

// The first field the change set that no longer holds what it was set to,
// as "progress is now 7, not 6"; empty when all of them do.
std::string ChangedSince(const Entry& now, const Entry& from, const std::optional<Entry>& to) {
	for (const auto& each : Columns()) {
		bool was_set = not to or each.text(from) != each.text(*to);
		if (was_set and each.text(now) != each.text(from))
			return std::format("{} is now {}, not {}", each.name, each.text(now), each.text(from));
	}
	return "";
}

}  // namespace

std::vector<Change> Diff(const std::vector<Entry>& saved, const std::vector<Entry>& current) {
	std::vector<Change> changes;
	for (const auto& now : current) {
		const Entry* was = FindById(saved, now.id);
		if (not was)
			changes.push_back({now.type, now.id, std::nullopt, now});
		else if (not SameFields(*was, now))
			changes.push_back({now.type, now.id, *was, now});
	}
	for (const auto& was : saved)
		if (not FindById(current, was.id))
			changes.push_back({was.type, was.id, was, std::nullopt});
	return changes;
}

std::vector<FieldChange> ChangedFields(const Change& made) {
	if (not made.before and not made.after)
		return {};
	if (not made.before)
		return {{"added", "-", StatusKey(made.after->status)}};
	if (not made.after)
		return {{"removed", StatusKey(made.before->status), "-"}};
	std::vector<FieldChange> fields;
	for (const auto& each : Columns()) {
		std::string before = each.text(*made.before);
		std::string after = each.text(*made.after);
		if (before != after)
			fields.push_back({each.name, before, after});
	}
	return fields;
}

Result<std::optional<Entry>> Transition(const std::vector<Entry>& list,
                                        const std::optional<Entry>& from,
                                        const std::optional<Entry>& to, bool force) {
	if (not from)
		return to ? ComeBack(list, *to) : std::nullopt;

	const Entry* now = FindById(list, from->id);
	if (not now) {
		// Gone already: nothing to remove, and nothing to set fields on.
		if (force and not to)
			return std::nullopt;
		return std::unexpected(std::format("{} is no longer in your lists", NameOf(*from)));
	}
	std::string moved = force ? "" : ChangedSince(*now, *from, to);
	if (not moved.empty())
		return std::unexpected(std::format("{} has changed since: {}", NameOf(*now), moved));
	if (not to)
		return std::nullopt;

	Entry next = *now;
	for (const auto& each : Columns())
		if (each.text(*from) != each.text(*to))
			each.copy(next, *to);
	next.updated_at = to->updated_at;
	return next;
}

}  // namespace catalist::journal
