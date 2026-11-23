#ifndef CATALIST_CLI_COMMON_HPP_
#define CATALIST_CLI_COMMON_HPP_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cli/cli.hpp"
#include "cli/output.hpp"
#include "core/entry.hpp"
#include "core/lists.hpp"
#include "core/result.hpp"
#include "storage/journal.hpp"

// What the commands of the command line share, and the commands
// themselves: each file named below holds a few commands, their handlers
// and their rows of the table. cli.cpp puts the table together and runs it.
namespace catalist::cli {

// ---- Messages ----

// "error: <message>" on standard error; returns 1, the exit code of a
// failure.
int Fail(CliContext& ctx, const std::string& message);
// The message, the command's usage line and where to read more; returns 2,
// the exit code of a wrong command line.
int UsageError(CliContext& ctx, const Command& cmd, const std::string& message);
int SaveFailed(CliContext& ctx);

// ---- A title as text ----

// The fields of a title, in the order every tab-separated line has them. A
// new field goes at the end: scripts count on the order.
[[nodiscard]] std::vector<std::string> RowOf(const Entry& e);
// Everything about a title: the fields of RowOf, then total_volumes,
// anilist_id, mal_id, its AniList page and its romaji name, as show prints it.
[[nodiscard]] std::vector<std::string> FullRowOf(const Entry& e);
// The terminal's colour for each status.
[[nodiscard]] const char* StatusColor(WatchStatus status);
// "episodes", "chapter".
[[nodiscard]] std::string UnitName(MediaType type, int count = 2);
// "1 title", "3 titles".
[[nodiscard]] std::string Titles(size_t count);
// "5/28", or "5/?" while the total is not known.
[[nodiscard]] std::string OutOf(int done, int total);
// Rows as tab-separated lines. A write that fails, because the reader has
// gone, ends it.
void WriteRows(CliContext& ctx, const std::vector<std::vector<std::string>>& rows);

// The statuses in the order they are listed, and the words they are
// written with: watching, completed, on-hold, dropped, planned and
// rewatching.
extern const std::vector<WatchStatus> kStatuses;
// A status as the command line takes it: one of those words, "reading" for
// watching or "rereading" for rewatching, in capitals or not. Nothing for any
// other word: the command line guesses nothing (import reads MyAnimeList's
// spellings).
[[nodiscard]] std::optional<WatchStatus> StatusNamed(const std::string& word);

// ---- Naming titles ----

// The operands of a command that takes one text (ls, search and add), with a
// space between them. Empty without operands.
[[nodiscard]] std::string ReadText(const args::Parsed& line);

// The titles named by words: a comma ends a name, and a ref or a word the
// shell kept whole (it holds a space) is a name of its own.
[[nodiscard]] std::vector<std::string> NamesIn(const std::vector<std::string>& words);

// -a and -m: the one list a command is kept to, or both.
[[nodiscard]] std::optional<MediaType> OnlyType(const args::Parsed& line);

// How the titles of a command are matched: in which list, whether a name may
// stand for several titles, and whether the person can be asked which.
struct Naming {
	std::optional<MediaType> only;
	bool all = false;
	bool ask = false;
	// Among several matches at a terminal, the only one in these is taken.
	std::vector<WatchStatus> prefer{};
	// With no name at a terminal, the titles to choose from.
	std::vector<WatchStatus> pick_from{};
};

// The options of every command that takes titles: -a, -m and --all first.
[[nodiscard]] std::vector<args::Option> WithRefOptions(std::vector<args::Option> options);

// The names given, or the exit code when there is nothing to go on with.
struct RefRequest {
	std::vector<std::string> refs;
	Naming how;
	std::optional<int> stop;
};

// Reads the titles named by the operands.
[[nodiscard]] RefRequest ReadRefs(const Command& cmd, const args::Parsed& line, CliContext& ctx);
// The same, from some of them.
[[nodiscard]] RefRequest ReadRefsIn(const Command& cmd, const args::Parsed& line, CliContext& ctx,
                                    const std::vector<std::string>& words);

// Asks which of the numbered choices was meant; nothing when the answer is
// empty or not one of them.
[[nodiscard]] std::optional<size_t> AskWhich(CliContext& ctx, size_t count);

// Every title the names name, each once, in the order named. Nothing, with
// the reason on standard error, when any fails: a command works on all
// it was given, or on none.
[[nodiscard]] std::optional<std::vector<Entry>> Resolve(CliContext& ctx,
                                                        const std::vector<std::string>& given,
                                                        const Naming& how);

// Reads the lists; false, with the reason on standard error, when they
// cannot be read.
[[nodiscard]] bool Load(CliContext& ctx);
// Takes the catalogue for writing, then reads it: nothing another program
// does can come between what the command reads and what it saves.
[[nodiscard]] bool LoadForWrite(CliContext& ctx);
// Takes the catalogue for writing and returns the titles chosen before it
// was taken, as they are now. Nothing, with the reason on standard error,
// when one of them has been removed or its ref now names another title.
[[nodiscard]] std::optional<std::vector<Entry>> TakeForWrite(CliContext& ctx,
                                                             const std::vector<Entry>& chosen);

// ---- Changing titles ----

// A line for every field that changed. Into a pipe: the ref, the title, the
// field, its old value and its new one.
void PrintChanges(CliContext& ctx, const std::vector<journal::Change>& changes);

// Saves what a command changed as one undo step and prints it (not with
// -q). A command that changed nothing says so and saves nothing.
int SaveAndReport(CliContext& ctx, const args::Parsed& line,
                  const std::vector<journal::Change>& changes);

using TitleChange = std::function<Entry(const Entry&)>;
using TitleRefusal = std::function<std::optional<std::string>(const Entry&)>;

// Makes one change to each title, then saves and reports it.
int ApplyTo(CliContext& ctx, const args::Parsed& line, const std::vector<Entry>& titles,
            const TitleChange& change);

// Makes one change to every title named; every name is resolved
// before any title changes.
int ChangeTitles(const Command& self, const args::Parsed& line, CliContext& ctx,
                 const TitleChange& change, const TitleRefusal& refuse = nullptr);

// The N of -n, or `otherwise` when it is not given.
[[nodiscard]] Result<int> CountGiven(const args::Parsed& line, int otherwise);

// What the helps share.
extern const args::Option kQuiet;
extern const char* const kRefsHelp;
extern const char* const kDashNotRef;
extern const char* const kChangeFormat;

// ---- The commands, by file ----

// listing.cpp: ls, show and stats, which only read.
[[nodiscard]] std::vector<Command> ListingCommands();
// What `ls` lists, and how.
struct ListQuery {
	// Words the titles hold.
	std::string text{};
	// Any of these; all of them when empty.
	std::vector<WatchStatus> statuses{};
	std::optional<MediaType> only{};
	SortKey key = SortKey::kTitle;
	bool reverse = false;
	// The refs alone, one a line.
	bool refs_only = false;
	// Bare `catalist`, which says what to do next when it has nothing to show.
	bool is_home = false;
};
// Lists the titles the query asks for: a table at a terminal, else lines.
// The exit code: 1 when the lists cannot be read.
int PrintList(CliContext& ctx, const ListQuery& query);
// tracking.cpp: +, -, set, done, hold, drop, plan, resume, rewatch and rm.
[[nodiscard]] std::vector<Command> TrackingCommands();
// online.cpp: add and search, which ask AniList about titles.
[[nodiscard]] std::vector<Command> OnlineCommands();
// transfer.cpp: import and export.
[[nodiscard]] std::vector<Command> TransferCommands();
// history.cpp: undo, redo and log.
[[nodiscard]] std::vector<Command> HistoryCommands();

}  // namespace catalist::cli

#endif  // CATALIST_CLI_COMMON_HPP_
