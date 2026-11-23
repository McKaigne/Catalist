#include "cli/common.hpp"

#include <algorithm>
#include <format>
#include <ostream>

#include "cli/refs.hpp"
#include "core/text.hpp"

namespace catalist::cli {

namespace {

using output::Paint;

// How a save of this run is entered in the undo history: under the command
// as typed, cut to 60 characters.
TxNote UndoNote(const CliContext& ctx) {
	return {utf8::Clip(ctx.typed, 60), ctx.Stamp()};
}

void SayNoMatch(CliContext& ctx, const std::string& ref, const Naming& how) {
	std::string where = how.only ? std::format("your {} list", TypeKey(*how.only)) : "your lists";
	Fail(ctx, refs::Parse(ref) ? std::format("no title in {} has the ref {}", where, ref)
	                           : std::format("nothing in {} matches '{}'", where, ref));
	if (ref.size() > 1 and ref[0] == '-')
		ctx.err << "Options go before the first operand.\n";
	ctx.err << "See what is there with: catalist ls\n";
}

// The titles one REF names: one, or all it matches with --all. Nothing,
// with the reason on standard error, when it matches none or when the
// choice among several is not made.
std::optional<std::vector<Entry>> Named(CliContext& ctx, const std::string& ref,
                                        const Naming& how) {
	auto found = refs::Find(ctx.db, ref, how.only);
	if (found.empty()) {
		SayNoMatch(ctx, ref, how);
		return std::nullopt;
	}
	if (found.size() == 1 or how.all)
		return found;

	ctx.err << std::format("{}'{}' matches {} titles:\n", how.ask ? "" : "error: ", ref,
	                       found.size());
	for (size_t i = 0; i < found.size(); ++i)
		ctx.err << (how.ask ? std::format("  {:>2}", i + 1) : "") << "  " << refs::Of(found[i])
		        << "  " << DisplayName(found[i]) << "\n";
	if (not how.ask) {
		ctx.err << "Name one by its ref (-a and -m keep to one list), or pass --all for all of "
		           "them.\n";
		return std::nullopt;
	}
	auto picked = AskWhich(ctx, found.size());
	if (not picked) {
		ctx.err << "Cancelled.\n";
		return std::nullopt;
	}
	return std::vector<Entry>{found[*picked]};
}

// One changed field as a terminal shows it: "Odd Taxi  5 → 6/13" for the
// progress, "Odd Taxi  score 8 → 9" for the rest.
std::string ChangeText(const CliContext& ctx, const Entry& shown,
                       const journal::FieldChange& field) {
	std::string title = Paint(DisplayName(shown), output::kBold, ctx.color);
	if (field.field == "added")
		return std::format("{}  added as {} ({})\n", title, field.after, refs::Of(shown));
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
	                         std::to_string(e.mal_id), output::OrDash(PageOf(e))})
		row.push_back(more);
	return row;
}

// The terminal's colour for each status.
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

// "1 title", "3 titles".
std::string Titles(size_t count) {
	return std::format("{} {}", count, count == 1 ? "title" : "titles");
}

// "5/28", or "5/?" while the total is not known.
std::string OutOf(int done, int total) {
	return std::format("{}/{}", done, total > 0 ? std::to_string(total) : std::string("?"));
}

// Rows as tab-separated lines. A write that fails, because the reader has
// gone, ends it.
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

std::optional<std::string> ReadText(const Command& cmd, const args::Parsed& line, CliContext& ctx) {
	std::string text;
	for (size_t i = 0; i < line.operands.size(); ++i) {
		const std::string& word = line.operands[i];
		if (i > 0 and word.size() > 1 and word[0] == '-') {
			UsageError(ctx, cmd,
			           std::format("'{}' comes after the text; options go before it", word));
			return std::nullopt;
		}
		text += (i > 0 ? " " : "") + word;
	}
	return text;
}

// -a and -m: the one list a command is kept to, or both.
std::optional<MediaType> OnlyType(const args::Parsed& line) {
	if (line.Has("a") == line.Has("m"))
		return std::nullopt;
	return line.Has("a") ? MediaType::kAnime : MediaType::kManga;
}

// The options of every command that takes REFs.
std::vector<args::Option> WithRefOptions(std::vector<args::Option> options) {
	options.insert(options.begin(), {{'a', "", "", "look in the anime list only"},
	                                 {'m', "", "", "look in the manga list only"},
	                                 {0, "all", "", "take every title a REF matches"}});
	return options;
}

// Reads the REF operands.
RefRequest ReadRefs(const Command& cmd, const args::Parsed& line, CliContext& ctx) {
	RefRequest request;
	if (line.operands.empty()) {
		request.stop = UsageError(ctx, cmd, "name a title: a ref such as a:12, or part of it");
		return request;
	}
	request.refs = line.operands;
	request.how = {OnlyType(line), line.Has("all"), ctx.interactive};
	bool any_empty = std::any_of(request.refs.begin(), request.refs.end(),
	                             [](const std::string& ref) { return ref.empty(); });
	if (any_empty)
		request.stop = UsageError(ctx, cmd, "a REF cannot be empty");
	else if (std::find(request.refs.begin(), request.refs.end(), "-") != request.refs.end())
		request.stop = UsageError(ctx, cmd, kDashNotRef);
	return request;
}

// Asks which of the numbered choices was meant; nothing when the answer is
// empty or not one of them.
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

// Every title the REFs name, each once, in the order named. Nothing when
// any REF fails: a command works on all it was given, or on none.
std::optional<std::vector<Entry>> Resolve(CliContext& ctx, const std::vector<std::string>& given,
                                          const Naming& how) {
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

// Takes the catalogue for writing, then reads it: nothing another program
// does can come between what the command reads and what it saves.
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
			                      DisplayName(each), refs::Of(each)));
			return std::nullopt;
		}
		current.push_back(*now);
	}
	return current;
}

// A line for every field that changed. Into a pipe: the ref, the title, the
// field, its old value and its new one.
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

// Saves what a command changed as one undo step and prints it (not with
// -q). A command that changed nothing says so and saves nothing.
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

// Makes one change to every title the REFs name; every REF is resolved
// before any title changes, and before the catalogue is taken for writing:
// no other program waits while the person chooses.
int ChangeTitles(const Command& self, const args::Parsed& line, CliContext& ctx,
                 const TitleChange& change) {
	auto request = ReadRefs(self, line, ctx);
	if (request.stop)
		return *request.stop;
	if (not Load(ctx))
		return 1;
	auto found = Resolve(ctx, request.refs, request.how);
	if (not found)
		return 1;
	auto current = TakeForWrite(ctx, *found);
	return current ? ApplyTo(ctx, line, *current, change) : 1;
}

// The N of -n, or `otherwise` when it is not given.
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
    "A REF is a ref such as a:12, or part of a title. Every REF is looked up\n"
    "before anything changes: when one of them fails, nothing is changed.\n";

// Alone, - would match every title with a hyphen in it.
extern const char* const kDashNotRef =
    "'-' is not a REF: give a ref such as a:12, or part of a title";

extern const char* const kChangeFormat =
    "Each field that changed is printed. Into a pipe that is one line of\n"
    "fields with a tab between them: ref, title, field, old value, new value.\n"
    "What a command changed is one step for undo.\n";

}  // namespace catalist::cli
