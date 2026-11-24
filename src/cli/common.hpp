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

namespace catalist::cli {

int Fail(CliContext& ctx, const std::string& message);
int UsageError(CliContext& ctx, const Command& cmd, const std::string& message);
int SaveFailed(CliContext& ctx);

// The fields of a title, in the order every tab-separated line has them. A
// new field goes at the end: scripts count on the order.
[[nodiscard]] std::vector<std::string> RowOf(const Entry& e);
[[nodiscard]] std::vector<std::string> FullRowOf(const Entry& e);
[[nodiscard]] const char* StatusColor(WatchStatus status);
[[nodiscard]] std::string UnitName(MediaType type, int count = 2);
[[nodiscard]] std::string Titles(size_t count);
[[nodiscard]] std::string OutOf(int done, int total);
void WriteRows(CliContext& ctx, const std::vector<std::vector<std::string>>& rows);

extern const std::vector<WatchStatus> kStatuses;
[[nodiscard]] std::optional<WatchStatus> StatusNamed(const std::string& word);

[[nodiscard]] std::string ReadText(const args::Parsed& line);

// The titles named by words: a comma ends a name, and a ref or a word the
// shell kept whole (it holds a space) is a name of its own.
[[nodiscard]] std::vector<std::string> NamesIn(const std::vector<std::string>& words);

[[nodiscard]] std::optional<MediaType> OnlyType(const args::Parsed& line);

struct Naming {
	std::optional<MediaType> only;
	bool all = false;
	bool ask = false;
	std::vector<WatchStatus> prefer{};
	std::vector<WatchStatus> pick_from{};
};

[[nodiscard]] std::vector<args::Option> WithRefOptions(std::vector<args::Option> options);

struct RefRequest {
	std::vector<std::string> refs;
	Naming how;
	std::optional<int> stop;
};

[[nodiscard]] RefRequest ReadRefs(const Command& cmd, const args::Parsed& line, CliContext& ctx);
[[nodiscard]] RefRequest ReadRefsIn(const Command& cmd, const args::Parsed& line, CliContext& ctx,
                                    const std::vector<std::string>& words);

[[nodiscard]] std::optional<size_t> AskWhich(CliContext& ctx, size_t count);
void PrintChoices(CliContext& ctx, const std::vector<Entry>& titles);
inline constexpr size_t kMostOffered = 20;

[[nodiscard]] std::optional<std::vector<Entry>> Resolve(CliContext& ctx,
                                                        const std::vector<std::string>& given,
                                                        const Naming& how);

[[nodiscard]] bool Load(CliContext& ctx);
[[nodiscard]] bool LoadForWrite(CliContext& ctx);
[[nodiscard]] std::optional<std::vector<Entry>> TakeForWrite(CliContext& ctx,
                                                             const std::vector<Entry>& chosen);

void PrintChanges(CliContext& ctx, const std::vector<journal::Change>& changes);

int SaveAndReport(CliContext& ctx, const args::Parsed& line,
                  const std::vector<journal::Change>& changes);

using TitleChange = std::function<Entry(const Entry&)>;
using TitleRefusal = std::function<std::optional<std::string>(const Entry&)>;

int ApplyTo(CliContext& ctx, const args::Parsed& line, const std::vector<Entry>& titles,
            const TitleChange& change);

int ChangeTitles(const Command& self, const args::Parsed& line, CliContext& ctx,
                 const TitleChange& change, const TitleRefusal& refuse = nullptr);

[[nodiscard]] Result<int> CountGiven(const args::Parsed& line, int otherwise);

extern const args::Option kQuiet;
extern const char* const kRefsHelp;
extern const char* const kDashNotRef;
extern const char* const kChangeFormat;

[[nodiscard]] std::vector<Command> ListingCommands();
struct ListQuery {
	std::string text{};
	std::vector<WatchStatus> statuses{};
	std::optional<MediaType> only{};
	SortKey key = SortKey::kTitle;
	bool reverse = false;
	bool refs_only = false;
	bool is_home = false;
};
int PrintList(CliContext& ctx, const ListQuery& query);
[[nodiscard]] std::vector<Command> TrackingCommands();
[[nodiscard]] std::vector<Command> OnlineCommands();
[[nodiscard]] std::vector<Command> TransferCommands();
[[nodiscard]] std::vector<Command> HistoryCommands();
int RunMenu(CliContext& ctx);

}  // namespace catalist::cli

#endif  // CATALIST_CLI_COMMON_HPP_
