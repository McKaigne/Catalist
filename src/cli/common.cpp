#include "cli/common.hpp"

#include <algorithm>
#include <format>
#include <ostream>
#include <utility>

#include "cli/refs.hpp"
#include "core/text.hpp"

namespace catalist::cli {

namespace {

using output::Paint;

TxNote UndoNote(const CliContext& ctx) {
	return {utf8::Clip(ctx.typed, 60), ctx.Stamp()};
}

void SayNoMatch(CliContext& ctx, const std::string& ref, const Naming& how) {
	std::string where = how.only ? std::format("your {} list", TypeKey(*how.only)) : "your lists";
	Fail(ctx, refs::Parse(ref) ? std::format("no title in {} has the ref {}", where, ref)
	                           : std::format("nothing in {} matches '{}'", where, ref));
	auto closest = refs::Parse(ref) ? std::nullopt : refs::Closest(ctx.db, ref, how.only);
	if (closest)
		ctx.err << std::format("Did you mean {}?\n", DisplayName(*closest));
	else
		ctx.err << "See what is there with: catalist ls\n";
}

void PrintChoices(CliContext& ctx, const std::vector<Entry>& titles) {
	int width = 0;
	for (const auto& each : titles)
		width = std::max(width, utf8::Width(DisplayName(each)));
	for (size_t i = 0; i < titles.size(); ++i)
		ctx.err << std::format("  {:>2}  {}  {}  {}\n", i + 1,
		                       utf8::Fit(DisplayName(titles[i]), width), TypeKey(titles[i].type),
		                       OutOf(titles[i].progress, titles[i].total_units));
}

std::optional<std::vector<Entry>> PickTitle(CliContext& ctx, const Naming& how) {
	constexpr size_t kMostOffered = 20;
	std::vector<Entry> offered;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		if (how.only and *how.only != type)
			continue;
		for (const auto& each : ctx.db.GetEntries(type))
			if (std::ranges::contains(how.pick_from, each.status))
				offered.push_back(each);
	}
	if (offered.empty()) {
		Fail(ctx, "no title to choose from; name one");
		return std::nullopt;
	}
	SortEntries(offered, SortKey::kUpdated, false);
	size_t more = offered.size() > kMostOffered ? offered.size() - kMostOffered : 0;
	offered.resize(offered.size() - more);
	PrintChoices(ctx, offered);
	if (more > 0)
		ctx.err << std::format("  and {} more: name one to reach it\n", more);
	auto picked = AskWhich(ctx, offered.size());
	if (not picked) {
		ctx.err << "Cancelled.\n";
		return std::nullopt;
	}
	return std::vector<Entry>{offered[*picked]};
}

std::optional<std::vector<Entry>> Named(CliContext& ctx, const std::string& ref,
                                        const Naming& how) {
	auto found = refs::Find(ctx.db, ref, how.only);
	if (found.empty()) {
		SayNoMatch(ctx, ref, how);
		return std::nullopt;
	}
	if (found.size() == 1 or how.all)
		return found;
	if (how.ask) {
		std::vector<Entry> preferred;
		for (const auto& each : found)
			if (std::ranges::contains(how.prefer, each.status))
				preferred.push_back(each);
		if (preferred.size() == 1)
			return preferred;
	}

	ctx.err << std::format("{}'{}' matches {} titles:\n", how.ask ? "" : "error: ", ref,
	                       found.size());
	if (not how.ask) {
		for (const auto& each : found)
			ctx.err << "  " << refs::Of(each) << "  " << DisplayName(each) << "\n";
		ctx.err << "Name one by its ref (-a and -m keep to one list), or pass --all for all of "
		           "them.\n";
		return std::nullopt;
	}
	PrintChoices(ctx, found);
	auto picked = AskWhich(ctx, found.size());
	if (not picked) {
		ctx.err << "Cancelled.\n";
		return std::nullopt;
	}
	return std::vector<Entry>{found[*picked]};
}

std::string ChangeText(const CliContext& ctx, const Entry& shown,
                       const journal::FieldChange& field) {
	std::string title = Paint(DisplayName(shown), output::kBold, ctx.color);
	if (field.field == "added")
		return std::format("{}  added as {}\n", title, field.after);
	if (field.field == "removed")
		return std::format("{}  removed\n", title);
	if (field.field == "progress")
		return std::format("{}  {} → {}\n", title, field.before,
		                   OutOf(shown.progress, shown.total_units));
	return std::format("{}  {} {} → {}\n", title, Paint(field.field, output::kDim, ctx.color),
	                   field.before, field.after);
}

}  // namespace

int Fail(CliContext& ctx, const std::string& message) {
	ctx.err << "error: " << message << "\n";
	return 1;
}

int UsageError(CliContext& ctx, const Command& cmd, const std::string& message) {
	ctx.err << std::format("error: {}\nUsage: catalist {}\nRun 'catalist help {}' for more.\n",
	                       message, UsageLine(cmd), cmd.name);
	return 2;
}

int SaveFailed(CliContext& ctx) {
	return Fail(ctx, std::format("could not save the catalogue to {}", ctx.db.data_dir().string()));
}

std::vector<std::string> RowOf(const Entry& e) {
	return {refs::Of(e),
	        TypeKey(e.type),
	        DisplayName(e),
	        StatusKey(e.status),
	        std::to_string(e.score),
	        std::to_string(e.progress),
	        std::to_string(e.total_units),
	        std::to_string(e.volumes),
	        std::to_string(e.rewatched),
	        output::DateText(e.start_date),
	        output::DateText(e.finish_date),
	        output::DateText(e.updated_at)};
}

std::vector<std::string> FullRowOf(const Entry& e) {
	auto row = RowOf(e);
	for (const auto& more : {std::to_string(e.total_volumes), std::to_string(e.anilist_id),
	                         std::to_string(e.mal_id), output::OrDash(PageOf(e)), e.title})
		row.push_back(more);
	return row;
}

const char* StatusColor(WatchStatus status) {
	switch (status) {
		case WatchStatus::kWatching:
			return "32";
		case WatchStatus::kCompleted:
			return "35";
		case WatchStatus::kOnHold:
			return "33";
		case WatchStatus::kDropped:
			return "31";
		case WatchStatus::kPlanToWatch:
			return "90";
		case WatchStatus::kRewatching:
			return "36";
	}
	return nullptr;
}

std::string UnitName(MediaType type, int count) {
	if (type == MediaType::kAnime)
		return count == 1 ? "episode" : "episodes";
	return count == 1 ? "chapter" : "chapters";
}

std::string Titles(size_t count) {
	return std::format("{} {}", count, count == 1 ? "title" : "titles");
}

std::string OutOf(int done, int total) {
	return std::format("{}/{}", done, total > 0 ? std::to_string(total) : std::string("?"));
}

void WriteRows(CliContext& ctx, const std::vector<std::vector<std::string>>& rows) {
	for (const auto& row : rows) {
		if (not ctx.out)
			return;
		ctx.out << output::TsvLine(row);
	}
}

// The order of the first five is that of the columns of `stats`; a status
// that comes later goes at the end.
extern const std::vector<WatchStatus> kStatuses = {
    WatchStatus::kWatching, WatchStatus::kCompleted,   WatchStatus::kOnHold,
    WatchStatus::kDropped,  WatchStatus::kPlanToWatch, WatchStatus::kRewatching};

std::optional<WatchStatus> StatusNamed(const std::string& word) {
	std::string wanted = utf8::Lowercase(word);
	if (wanted == "reading")
		return WatchStatus::kWatching;
	if (wanted == "rereading")
		return WatchStatus::kRewatching;
	for (WatchStatus status : kStatuses)
		if (StatusKey(status) == wanted)
			return status;
	return std::nullopt;
}

std::string ReadText(const args::Parsed& line) {
	std::string text;
	for (const auto& word : line.operands)
		text += (text.empty() ? "" : " ") + word;
	return text;
}

std::vector<std::string> NamesIn(const std::vector<std::string>& words) {
	std::vector<std::string> names;
	std::string open;
	auto close = [&] {
		if (not open.empty())
			names.push_back(std::exchange(open, ""));
	};
	auto add = [&](const std::string& piece) {
		if (refs::Parse(piece)) {
			close();
			names.push_back(piece);
		} else if (not piece.empty()) {
			open += (open.empty() ? "" : " ") + piece;
		}
	};
	for (const auto& word : words) {
		if (word.contains(' ')) {
			close();
			names.push_back(word);
			continue;
		}
		size_t start = 0;
		for (size_t comma = word.find(','); comma != std::string::npos;
		     comma = word.find(',', start)) {
			add(word.substr(start, comma - start));
			close();
			start = comma + 1;
		}
		add(word.substr(start));
	}
	close();
	return names;
}

std::optional<MediaType> OnlyType(const args::Parsed& line) {
	if (line.Has("a") == line.Has("m"))
		return std::nullopt;
	return line.Has("a") ? MediaType::kAnime : MediaType::kManga;
}

std::vector<args::Option> WithRefOptions(std::vector<args::Option> options) {
	options.insert(options.begin(), {{'a', "", "", "look in the anime list only"},
	                                 {'m', "", "", "look in the manga list only"},
	                                 {0, "all", "", "take every title a name matches"}});
	return options;
}

RefRequest ReadRefs(const Command& cmd, const args::Parsed& line, CliContext& ctx) {
	return ReadRefsIn(cmd, line, ctx, line.operands);
}

RefRequest ReadRefsIn(const Command& cmd, const args::Parsed& line, CliContext& ctx,
                      const std::vector<std::string>& words) {
	RefRequest request;
	request.refs = NamesIn(words);
	std::vector<WatchStatus> prefer =
	    cmd.pick_from.empty() ? std::vector{WatchStatus::kWatching, WatchStatus::kRewatching}
	                          : cmd.pick_from;
	request.how = {OnlyType(line), line.Has("all"), ctx.interactive, prefer, cmd.pick_from};
	if (std::ranges::contains(words, std::string()))
		request.stop = UsageError(ctx, cmd, "a title cannot be empty");
	else if (request.refs.empty() and (cmd.pick_from.empty() or not ctx.interactive))
		request.stop = UsageError(ctx, cmd, "name a title: any part of its name");
	else if (std::ranges::any_of(request.refs, [](const std::string& name) {
		         return not refs::Parse(name) and utf8::Folded(name).empty();
	         }))
		request.stop = UsageError(ctx, cmd, kDashNotRef);
	return request;
}

std::optional<size_t> AskWhich(CliContext& ctx, size_t count) {
	ctx.err << std::format("Which one? [1-{}, Enter to cancel]: ", count) << std::flush;
	std::string answer;
	if (not std::getline(ctx.in, answer))
		return std::nullopt;
	auto picked = args::ParseCount(answer);
	if (not picked or *picked < 1 or static_cast<size_t>(*picked) > count)
		return std::nullopt;
	return static_cast<size_t>(*picked - 1);
}

std::optional<std::vector<Entry>> Resolve(CliContext& ctx, const std::vector<std::string>& given,
                                          const Naming& how) {
	if (given.empty())
		return PickTitle(ctx, how);
	std::vector<Entry> all;
	for (const auto& ref : given) {
		auto found = Named(ctx, ref, how);
		if (not found)
			return std::nullopt;
		for (const auto& each : *found) {
			bool taken = std::any_of(all.begin(), all.end(), [&each](const Entry& other) {
				return other.type == each.type and other.id == each.id;
			});
			if (not taken)
				all.push_back(each);
		}
	}
	return all;
}

bool Load(CliContext& ctx) {
	auto loaded = ctx.db.LoadAll();
	if (not loaded.has_value())
		Fail(ctx, loaded.error());
	return loaded.has_value();
}

bool LoadForWrite(CliContext& ctx) {
	auto held = ctx.db.BeginWrite();
	if (not held.has_value()) {
		Fail(ctx, held.error());
		return false;
	}
	return Load(ctx);
}

// A ref can pass to a new title once its own is removed, so a title is the
// one that was chosen only if its name and site ids are still the same.
std::optional<std::vector<Entry>> TakeForWrite(CliContext& ctx, const std::vector<Entry>& chosen) {
	if (not LoadForWrite(ctx))
		return std::nullopt;
	std::vector<Entry> current;
	for (const auto& each : chosen) {
		auto now = ctx.db.GetById(each.type, each.id);
		bool same = now and now->title == each.title and now->anilist_id == each.anilist_id and
		            now->mal_id == each.mal_id;
		if (not same) {
			Fail(ctx, std::format("{} ({}) was removed or replaced meanwhile; nothing was changed",
			                      DisplayName(each), TypeKey(each.type)));
			return std::nullopt;
		}
		current.push_back(*now);
	}
	return current;
}

void PrintChanges(CliContext& ctx, const std::vector<journal::Change>& changes) {
	for (const auto& made : changes) {
		const Entry& shown = made.after ? *made.after : *made.before;
		for (const auto& field : journal::ChangedFields(made)) {
			if (not ctx.out)
				return;
			ctx.out << (ctx.columns > 0
			                ? ChangeText(ctx, shown, field)
			                : output::TsvLine({refs::Of(shown), DisplayName(shown), field.field,
			                                   field.before, field.after}));
		}
	}
}

int SaveAndReport(CliContext& ctx, const args::Parsed& line,
                  const std::vector<journal::Change>& changes) {
	bool any = std::any_of(changes.begin(), changes.end(), [](const journal::Change& made) {
		return not journal::ChangedFields(made).empty();
	});
	if (not any) {
		if (not line.Has("q"))
			ctx.err << "Nothing changed.\n";
		return 0;
	}
	if (not ctx.db.SaveAll(UndoNote(ctx)))
		return SaveFailed(ctx);
	if (not line.Has("q"))
		PrintChanges(ctx, changes);
	return 0;
}

int ApplyTo(CliContext& ctx, const args::Parsed& line, const std::vector<Entry>& titles,
            const TitleChange& change) {
	std::vector<journal::Change> changes;
	for (const auto& before : titles) {
		Entry after = change(before);
		ctx.db.UpdateEntry(after);
		changes.push_back({before.type, before.id, before, after});
	}
	return SaveAndReport(ctx, line, changes);
}

// Every name is resolved before the catalogue is taken for writing: no other
// program waits while the person chooses.
int ChangeTitles(const Command& self, const args::Parsed& line, CliContext& ctx,
                 const TitleChange& change, const TitleRefusal& refuse) {
	auto request = ReadRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not Load(ctx))
		return 1;
	auto found = Resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;
	auto current = TakeForWrite(ctx, *found);
	if (not current)
		return 1;
	for (const auto& each : *current) {
		auto reason = refuse ? refuse(each) : std::nullopt;
		if (reason)
			return Fail(ctx, *reason);
	}
	return ApplyTo(ctx, line, *current, change);
}

Result<int> CountGiven(const args::Parsed& line, int otherwise) {
	if (not line.Has("n"))
		return otherwise;
	auto count = args::ParseCount(line.Get("n"));
	if (not count or *count == 0)
		return std::unexpected(std::format("-n '{}' is not a count of one or more", line.Get("n")));
	return *count;
}

extern const args::Option kQuiet{'q', "", "", "print nothing"};

extern const char* const kRefsHelp =
    "A TITLE is any part of its English or romaji name, the words in any order.\n"
    "Commas separate several. All are looked up before anything changes: when\n"
    "one of them fails, nothing is changed.\n";

extern const char* const kDashNotRef =
    "punctuation alone is not a title: give part of its name";

extern const char* const kChangeFormat =
    "Each field that changed is printed. Into a pipe that is one line of\n"
    "fields with a tab between them: ref, title, field, old value, new value.\n"
    "What a command changed is one step for undo.\n";

}  // namespace catalist::cli
