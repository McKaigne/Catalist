#include <algorithm>
#include <format>
#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "core/text.hpp"
#include "storage/accounts.hpp"

namespace catalist::cli {

namespace {

// The list `add` and `search` work in: anime unless -m says manga. Nothing
// when both -a and -m are given.
std::optional<MediaType> OneType(const args::Parsed& line) {
	if (line.Has("a") and line.Has("m"))
		return std::nullopt;
	return line.Has("m") ? MediaType::kManga : MediaType::kAnime;
}

// The title as the site has it: its name, the ids it knows, the totals and
// how much is out. Fails with what to tell the person.
Result<Entry> LookedUp(CliContext& ctx, web::Tracker& site, Entry blank, const web::Match& match) {
	blank.anilist_id = match.anilist_id;
	blank.mal_id = match.mal_id;
	blank.kitsu_id = match.kitsu_id;
	auto details = site.LookUp(blank);
	if (not details.has_value())
		return std::unexpected(WebError(details.error()));
	blank.title = details.value().name.empty() ? match.name : details.value().name;
	return ApplyWebFacts(blank, details.value().facts, ctx.Today());
}

// A title added by hand has no id to know it by: its name is all there is
// to warn about.
void NoteSameName(CliContext& ctx, const Entry& added) {
	std::string name = utf8::Lowercase(added.title);
	for (const auto& e : ctx.db.GetEntries(added.type)) {
		if (utf8::Lowercase(e.title) != name)
			continue;
		ctx.err << std::format("note: your {} list has a title of that name already, {}.\n",
		                       TypeKey(added.type), refs::Of(e));
		return;
	}
}

int RunAdd(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto type = OneType(line);
	if (not type)
		return UsageError(ctx, self, "give -a or -m, not both");
	auto name = ReadText(self, line, ctx);
	if (not name)
		return 2;
	if (name->empty())
		return UsageError(ctx, self, "name the title to add");
	auto status = StatusNamed(line.Has("status") ? line.Get("status") : "planned");
	if (not status)
		return UsageError(ctx, self,
		                  std::format("unknown status '{}'{}", line.Get("status"),
		                              DidYouMean(line.Get("status"), StatusWords())));
	std::optional<int> pick;
	if (line.Has("pick")) {
		pick = args::ParseCount(line.Get("pick"));
		if (not pick or *pick == 0)
			return UsageError(
			    ctx, self,
			    std::format("--pick '{}' is not the number of a match", line.Get("pick")));
	}
	std::optional<int> score;
	if (line.Has("score")) {
		score = args::ParseCount(line.Get("score"));
		if (not score or *score > 10)
			return UsageError(ctx, self,
			                  std::format("--score '{}' is not 0 to 10", line.Get("score")));
	}
	std::optional<int> progress;
	if (line.Has("progress")) {
		progress = args::ParseCount(line.Get("progress"));
		if (not progress)
			return UsageError(ctx, self,
			                  std::format("--progress '{}' is not a count", line.Get("progress")));
	}
	bool by_hand = line.Has("no-lookup");
	if (by_hand and (pick or line.Has("from")))
		return UsageError(ctx, self,
		                  std::format("{} chooses among a site's matches; --no-lookup asks none",
		                              pick ? "--pick" : "--from"));
	if (not FromKnown(self, line, ctx))
		return 2;

	IsoDate today = ctx.Today();
	Entry added;
	added.type = *type;
	added.title = *name;
	added.status = WatchStatus::kPlanToWatch;
	added.updated_at = today;
	// The site is asked before the catalogue is taken for writing: no other
	// program waits while the person chooses.
	if (not by_hand) {
		if (AsksProvider(line) and not Load(ctx))
			return 1;
		auto site = TrackerFor(ctx, SiteOf(line, ctx));
		auto match = ChooseMatch(ctx, *site, *type, added.title, pick);
		if (not match)
			return 1;
		auto found = LookedUp(ctx, *site, added, *match);
		if (not found.has_value())
			return Fail(ctx, found.error());
		added = found.value();
	}
	// In the order `set` takes them: the progress starts the title, or
	// completes it at the total, and a status that is given stands.
	if (progress)
		added = SetCount(added, CountField::kProgress, *progress, today);
	if (line.Has("status") or not progress)
		added = SetStatus(added, *status, today);
	if (score)
		added = SetScore(added, *score, today);

	if (not LoadForWrite(ctx))
		return 1;
	if (auto mine = ctx.db.FindByIds(*type, added.anilist_id, added.mal_id, added.kitsu_id)) {
		ctx.err << std::format("{} is in your {} list already, as {} ({}).\nNothing changed.\n",
		                       mine->title, TypeKey(*type), refs::Of(*mine),
		                       StatusKey(mine->status));
		return 0;
	}
	if (by_hand)
		NoteSameName(ctx, added);
	added.id = ctx.db.AddEntry(added);
	return SaveAndReport(ctx, line, {{added.type, added.id, std::nullopt, added}});
}

// The columns of a match of `search`. `pick` is its place in the list, the
// N of `add --pick=N`; `ref` is the title in your lists, when it is there.
const std::vector<std::string> kSearchColumns = {
    "pick", "type", "title", "year", "total", "score", "anilist_id", "mal_id", "ref", "kitsu_id"};

// The site's score out of 10, to one decimal; 0 while there is none.
output::Cell ScoreCell(double score) {
	if (score <= 0.0)
		return {"0", output::JsonForm::kNull};
	return {std::format("{:.1f}", score), output::JsonForm::kNumber};
}

// The matches as a table for a terminal.
std::string MatchTable(const CliContext& ctx, MediaType type,
                       const std::vector<web::Match>& matches,
                       const std::vector<std::string>& mine) {
	std::vector<output::TableColumn> columns = {
	    {"#", true},
	    {"YEAR"},
	    {"TITLE", false, true},
	    {type == MediaType::kAnime ? "EPISODES" : "CHAPTERS", true},
	    {"SCORE", true},
	    {"YOURS"}};
	std::vector<std::vector<output::Styled>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		rows.push_back({{std::to_string(i + 1), output::kCyan},
		                {match.year.empty() ? "-" : match.year},
		                {match.name},
		                {match.total_units > 0 ? std::to_string(match.total_units) : "?"},
		                {match.score > 0.0 ? std::format("{:.1f}", match.score) : "-"},
		                {mine[i], output::kGreen}});
	}
	return output::Table(columns, rows, ctx.columns, ctx.color);
}

int RunSearch(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto type = OneType(line);
	if (not type)
		return UsageError(ctx, self, "give -a or -m, not both");
	auto query = ReadText(self, line, ctx);
	if (not query)
		return 2;
	if (query->empty())
		return UsageError(ctx, self, "give the QUERY to search for");
	if (not FromKnown(self, line, ctx))
		return 2;
	if (not Load(ctx))
		return 1;
	SendWaiting(ctx);
	auto site = TrackerFor(ctx, SiteOf(line, ctx));
	auto found = SearchSite(ctx, *site, *type, *query);
	if (not found.has_value())
		return Fail(ctx, WebError(found.error()));
	const auto& matches = found.value();
	std::vector<std::string> mine;
	for (const auto& match : matches) {
		auto tracked = ctx.db.FindByIds(*type, match.anilist_id, match.mal_id, match.kitsu_id);
		mine.push_back(tracked ? refs::Of(*tracked) : "");
	}

	if (not line.Has("json") and ctx.columns > 0) {
		if (matches.empty()) {
			ctx.err << std::format("{} has no {} matching '{}'.\n", ProviderName(site->Site()),
			                       TypeKey(*type), *query);
			return 0;
		}
		ctx.out << MatchTable(ctx, *type, matches, mine);
		ctx.err << std::format("Add one with: catalist add{}{} --pick=N QUERY\n",
		                       *type == MediaType::kManga ? " -m" : "",
		                       line.Has("from") ? " --from=" + line.Get("from") : "");
		return 0;
	}
	std::vector<std::vector<output::Cell>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		rows.push_back({output::Number(static_cast<long long>(i) + 1), output::Text(TypeKey(*type)),
		                output::Text(match.name),
		                match.year.empty() ? output::Maybe("")
		                                   : output::Cell{match.year, output::JsonForm::kNumber},
		                output::Known(match.total_units), ScoreCell(match.score),
		                output::Known(match.anilist_id), output::Known(match.mal_id),
		                output::Maybe(mine[i]), output::Known(match.kitsu_id)});
	}
	WriteRows(ctx, kSearchColumns, rows, line.Has("json"), line.Has("header"));
	return 0;
}

// "a:1, m:3".
std::string JoinedWith(const std::vector<std::string>& words, const std::string& between) {
	std::string all;
	for (const auto& word : words)
		all += (all.empty() ? "" : between) + word;
	return all;
}

// What refresh checks when no REF is given: the titles being followed.
bool Followed(const Entry& e) {
	return e.status == WatchStatus::kWatching or e.status == WatchStatus::kRewatching or
	       e.status == WatchStatus::kOnHold;
}

// The titles refresh is to check, read from the lists; nothing, with the
// exit code, when there are none to go on with.
struct RefreshList {
	std::vector<Entry> titles;
	std::optional<int> stop;
};

RefreshList TitlesToRefresh(const Command& self, const args::Parsed& line, CliContext& ctx) {
	RefreshList wanted;
	if (not line.operands.empty()) {
		auto request = ReadRefs(self, line, ctx);
		if (request.stop) {
			wanted.stop = request.stop;
			return wanted;
		}
		auto found = Load(ctx) ? Resolve(ctx, request.refs, request.how) : std::nullopt;
		if (not found)
			wanted.stop = 1;
		else
			wanted.titles = *found;
		return wanted;
	}
	if (not Load(ctx)) {
		wanted.stop = 1;
		return wanted;
	}
	auto only = OnlyType(line);
	for (MediaType type : {MediaType::kAnime, MediaType::kManga})
		for (const auto& e : ctx.db.GetEntries(type))
			if ((not only or *only == type) and Followed(e))
				wanted.titles.push_back(e);
	return wanted;
}

int RunRefresh(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (not FromKnown(self, line, ctx))
		return 2;
	auto wanted = TitlesToRefresh(self, line, ctx);
	if (wanted.stop)
		return *wanted.stop;
	bool quiet = line.Has("q");
	if (wanted.titles.empty()) {
		if (not quiet)
			ctx.err << "Nothing is being watched or read: name the titles to refresh.\n";
		return 0;
	}

	// The site is asked before the catalogue is taken for writing, as by
	// add: nothing waits on the network.
	SendWaiting(ctx);
	auto site = TrackerFor(ctx, SiteOf(line, ctx));
	std::string name = ProviderName(site->Site());
	if (not quiet and wanted.titles.size() > 1 and site->Site() != Provider::kAniList)
		ctx.err << std::format("Asking {} about {}, one a second.\n", name,
		                       Titles(wanted.titles.size()));
	auto answers = site->LookUpMany(wanted.titles);
	std::vector<std::string> unknown;
	for (size_t i = 0; i < answers.size(); ++i) {
		if (answers[i].has_value())
			continue;
		if (answers[i].error().why == web::Failure::Kind::kUnreachable)
			return Fail(ctx, WebError(answers[i].error()));
		unknown.push_back(refs::Of(wanted.titles[i]));
	}

	if (not LoadForWrite(ctx))
		return 1;
	IsoDate today = ctx.Today();
	std::vector<journal::Change> changes;
	size_t fresh = 0;
	for (size_t i = 0; i < answers.size(); ++i) {
		const Entry& asked = wanted.titles[i];
		auto now = answers[i].has_value() ? ctx.db.GetById(asked.type, asked.id) : std::nullopt;
		// Removed meanwhile by another program: nothing to refresh.
		if (not now)
			continue;
		Entry after = ApplyWebFacts(*now, answers[i].value().facts, today);
		ctx.db.UpdateEntry(after);
		changes.push_back({after.type, after.id, *now, after});
		fresh += NewReleases(after) > 0 ? 1 : 0;
	}
	// What is out and the ids are kept, but only a new total is a step for
	// undo, as for import.
	bool shown = std::any_of(changes.begin(), changes.end(), [](const journal::Change& made) {
		return not journal::ChangedFields(made).empty();
	});
	if (shown) {
		int code = SaveAndReport(ctx, line, changes);
		if (code != 0)
			return code;
	} else if (not ctx.db.SaveAll()) {
		return SaveFailed(ctx);
	}
	if (quiet)
		return 0;
	if (not unknown.empty())
		ctx.err << std::format("note: {} could not find {}.\n", name, JoinedWith(unknown, ", "));
	ctx.err << std::format("Checked {} on {}{}.\n", Titles(changes.size()), name,
	                       fresh == 0
	                           ? ""
	                           : std::format("; {} new episodes or chapters out",
	                                         fresh == 1 ? "1 has" : std::format("{} have", fresh)));
	return 0;
}

}  // namespace

namespace {

// "28 episodes", or "? chapters" while the total is not known.
std::string LengthOf(const web::Match& match, MediaType type) {
	return std::format("{} {}", match.total_units > 0 ? std::to_string(match.total_units) : "?",
	                   UnitName(type, match.total_units));
}

}  // namespace

web::Answer<std::vector<web::Match>> SearchSite(CliContext& ctx, web::Tracker& site, MediaType type,
                                                const std::string& query) {
	Provider where = site.Site();
	if (query.starts_with("id:")) {
		auto id = args::ParseCount(query.substr(3));
		if (not id or *id == 0)
			return web::Refused(std::format("'{}' is not id: and a number", query));
		Entry probe;
		probe.type = type;
		if (where == Provider::kAniList)
			probe.anilist_id = *id;
		else if (where == Provider::kMyAnimeList)
			probe.mal_id = *id;
		else
			probe.kitsu_id = *id;
		auto found = site.LookUp(probe);
		if (not found and found.error().why == web::Failure::Kind::kRefused)
			return std::vector<web::Match>{};
		if (not found)
			return std::unexpected(found.error());
		const auto& facts = found->facts;
		return std::vector<web::Match>{{facts.anilist_id, facts.mal_id, facts.kitsu_id, found->name,
		                                facts.total_units, facts.total_volumes, "", 0.0}};
	}
	if (not query.starts_with("my:"))
		return site.Search(type, query);
	auto login = accounts::On(ReadAccounts(ctx), where);
	if (not login)
		return web::Refused(
		    std::format("my: searches your list on {}: log in first with "
		                "'catalist login {}'",
		                ProviderName(where), ProviderKey(where)));
	auto list = site.Pull(type, login->user);
	if (not list)
		return std::unexpected(list.error());
	std::string words = query.substr(3);
	std::vector<web::Match> found;
	for (const auto& e : *list) {
		if (not refs::TitleHolds(e, words))
			continue;
		found.push_back(
		    {e.anilist_id, e.mal_id, e.kitsu_id, e.title, e.total_units, e.total_volumes, "", 0.0});
		if (found.size() == 10)
			break;
	}
	return found;
}

// The match `add` is to take: the only one, the one --pick names, or the one
// chosen at a terminal. Nothing, with the reason on standard error, when
// there is no such match.
std::optional<web::Match> ChooseMatch(CliContext& ctx, web::Tracker& site, MediaType type,
                                      const std::string& query, std::optional<int> pick) {
	std::string name = ProviderName(site.Site());
	auto found = SearchSite(ctx, site, type, query);
	if (not found.has_value()) {
		Fail(ctx, WebError(found.error()));
		ctx.err << std::format("To add a title without {}: catalist add --no-lookup TITLE\n", name);
		return std::nullopt;
	}
	const auto& matches = found.value();
	if (matches.empty()) {
		Fail(ctx, std::format("{} has no {} matching '{}'", name, TypeKey(type), query));
		return std::nullopt;
	}
	if (pick and static_cast<size_t>(*pick) > matches.size()) {
		Fail(ctx, std::format(
		              "--pick={}, but {} has only {} for '{}'", *pick, name,
		              matches.size() == 1 ? "one match" : std::format("{} matches", matches.size()),
		              query));
		return std::nullopt;
	}
	if (pick)
		return matches[static_cast<size_t>(*pick) - 1];
	if (matches.size() == 1)
		return matches[0];

	ctx.err << std::format("{}'{}' matches {} {} on {}:\n", ctx.interactive ? "" : "error: ", query,
	                       matches.size(), TypeKey(type), name);
	for (size_t i = 0; i < matches.size(); ++i)
		ctx.err << std::format("  {:>2}  {:<4}  {}  ({})\n", i + 1,
		                       matches[i].year.empty() ? "-" : matches[i].year, matches[i].name,
		                       LengthOf(matches[i], type));
	if (not ctx.interactive) {
		ctx.err << "Take one by its number with --pick=N.\n";
		return std::nullopt;
	}
	auto chosen = AskWhich(ctx, matches.size());
	if (not chosen) {
		ctx.err << "Cancelled.\n";
		return std::nullopt;
	}
	return matches[*chosen];
}

std::vector<Command> OnlineCommands() {
	return {
	    {.name = "add",
	     .summary = "Add a title found on AniList, MyAnimeList or Kitsu",
	     .details =
	         std::string("Searches your provider (see 'provider'), or the site --from names, for\n"
	                     "TITLE, an anime unless -m is given, and adds the match: the only one,\n"
	                     "the one you choose when there are several, or the one --pick names by\n"
	                     "its number in that list ('search' prints the same list). Where nobody\n"
	                     "can be asked, several matches without --pick are an error. A new title\n"
	                     "is planned, or watching once it has a --progress, unless --status says\n"
	                     "otherwise. A title that is in your list already is left as it is.\n\n"
	                     "With --no-lookup no site is asked: TITLE is added as it is written,\n"
	                     "without ids or totals ('set' can give it a total).\n\n") +
	         kChangeFormat,
	     .options = {{'a', "", "", "add an anime (the default)"},
	                 {'m', "", "", "add a manga"},
	                 kFrom,
	                 {0, "pick", "N", "take the Nth match without asking"},
	                 {0, "status", "STATUS",
	                  "watching, completed, on-hold, dropped, planned or rewatching",
	                  StatusWords()},
	                 {0, "score", "N", "give it a score as well, 0 to 10"},
	                 {0, "progress", "N", "the episodes or chapters done already"},
	                 {0, "no-lookup", "", "add TITLE as written, without asking a site"},
	                 kQuiet},
	     .operands = "TITLE...",
	     .run = RunAdd,
	     .completes = OperandKind::kText},
	    {.name = "search",
	     .summary = "Search a site without adding anything",
	     .details = "Prints up to ten matches for QUERY on your provider, or the site --from\n"
	                "names, anime unless -m is given, best first. At a terminal they are\n"
	                "drawn as a table. Into a pipe each is one line of fields with a tab\n"
	                "between them:\n"
	                "  pick type title year total score anilist_id mal_id ref kitsu_id\n"
	                "pick is the N of 'add --pick=N'; ref is the title in your lists, or '-'.\n"
	                "A year that is not known is '-', a total, a score or an id that is not\n"
	                "is 0. Each site knows its own ids; AniList also MyAnimeList's, and Kitsu\n"
	                "both of the others'.\n",
	     .options = {{'a', "", "", "search anime (the default)"},
	                 {'m', "", "", "search manga"},
	                 kFrom,
	                 {0, "json", "", "print one JSON object a line"},
	                 {0, "header", "", "name the columns on the first line of a pipe"}},
	     .operands = "QUERY...",
	     .run = RunSearch,
	     .completes = OperandKind::kText},
	    {.name = "refresh",
	     .summary = "Ask a site what is out of the titles you follow",
	     .details =
	         std::string("Asks your provider, or the site --from names, for each title's totals\n"
	                     "and how many of its episodes or chapters are out (what the new column\n"
	                     "of ls counts from), and fills in the ids that site knows. Without a\n"
	                     "REF it asks about the titles you are watching or reading, or have on\n"
	                     "hold. AniList answers fifty titles a request; MyAnimeList and Kitsu\n"
	                     "one a second, and only AniList says how much of a running show has\n"
	                     "aired. A changed total is a step for undo.\n\n") +
	         kRefsHelp + "\n" + kChangeFormat,
	     .options = WithRefOptions({kFrom, kQuiet}),
	     .operands = "[REF...]",
	     .run = RunRefresh,
	     .completes = OperandKind::kRefs},
	};
}

}  // namespace catalist::cli
