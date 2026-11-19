#include <algorithm>
#include <format>
#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "storage/accounts.hpp"

// The track sheet, as Mihon has one for each series: the title as each site
// has it, read there, beside the lists; and what can be done about one site
// from there (hide the title, or take the site's entry as the title's).
namespace catalist::cli {

namespace {

// One site's line of the sheet.
struct SiteLine {
	Provider site = Provider::kAniList;
	int id = 0;
	// listed, missing (not on your list there), unlinked, logged-out or
	// failed.
	std::string state{};
	std::string why{};
	web::SiteEntry theirs{};
	// The fields where the site differs from the lists.
	std::vector<std::string> differs{};
};

// The fields where your entry on the site is not as the title has it.
std::vector<std::string> Differences(const Entry& mine, const web::SiteEntry& theirs,
                                     Provider site) {
	Entry there = AsTitle(theirs, site);
	std::vector<std::string> fields;
	auto check = [&fields](bool same, const char* name) {
		if (not same)
			fields.emplace_back(name);
	};
	check(there.status == mine.status, "status");
	check(there.score == mine.score, "score");
	check(there.progress == mine.progress, "progress");
	if (mine.type == MediaType::kManga and site != Provider::kKitsu)
		check(there.volumes == mine.volumes, "volumes");
	check(there.rewatched == mine.rewatched, "rewatched");
	check(there.start_date == mine.start_date, "started");
	check(there.finish_date == mine.finish_date, "finished");
	return fields;
}

// Each site's line: read there when the title is linked and you are logged
// in.
std::vector<SiteLine> ReadSites(CliContext& ctx, const Entry& title) {
	auto logins = ReadAccounts(ctx);
	std::vector<SiteLine> lines;
	for (Provider site : AllSites()) {
		SiteLine line{site, IdOn(title, site)};
		auto login = accounts::On(logins, site);
		if (line.id == 0) {
			line.state = "unlinked";
		} else if (not login) {
			line.state = "logged-out";
		} else {
			auto tracker = TrackerFor(ctx, site);
			auto fresh = CurrentLogin(ctx, *tracker, *login);
			auto yours =
			    fresh ? tracker->EntryOf(title, *fresh)
			          : web::Answer<std::optional<web::SiteEntry>>(std::unexpected(fresh.error()));
			if (not yours) {
				line.state = "failed";
				line.why = WebError(yours.error());
			} else if (not yours->has_value()) {
				line.state = "missing";
			} else {
				line.state = "listed";
				line.theirs = **yours;
				line.differs = Differences(title, line.theirs, site);
			}
		}
		lines.push_back(line);
	}
	return lines;
}

bool Differs(const SiteLine& line, const std::string& field) {
	return std::find(line.differs.begin(), line.differs.end(), field) != line.differs.end();
}

std::string DayText(const std::optional<IsoDate>& day) {
	return day ? day->ToString() : "-";
}

// The sheet at a terminal: the lists' line, then a line a site, what
// differs from the lists in red.
void PrintSheet(CliContext& ctx, const Entry& title, const std::vector<SiteLine>& lines) {
	std::vector<std::vector<output::Styled>> rows;
	rows.push_back({{"Catalist", output::kBold},
	                {refs::Of(title), output::kCyan},
	                {StatusToString(title.status, title.type)},
	                {output::Stars(title.score), output::kYellow},
	                {OutOf(title.progress, title.total_units)},
	                {DayText(title.start_date)},
	                {DayText(title.finish_date)},
	                {""}});
	for (const auto& line : lines) {
		std::string name = ProviderName(line.site);
		std::string id = line.id > 0 ? std::to_string(line.id) : "-";
		if (line.state != "listed") {
			std::string said = line.state == "unlinked"     ? "not linked"
			                   : line.state == "logged-out" ? "not logged in"
			                   : line.state == "missing"    ? "not on your list"
			                                                : line.why;
			rows.push_back({{name, output::kBold}, {id}, {said, output::kDim}});
			continue;
		}
		auto mark = [&line](const std::string& field, const char* otherwise) {
			return Differs(line, field) ? output::kRed : otherwise;
		};
		const web::SiteEntry& theirs = line.theirs;
		rows.push_back({{name, output::kBold},
		                {id},
		                {StatusToString(theirs.status, title.type), mark("status", nullptr)},
		                {output::Stars(web::OutOfTen(line.site, theirs.score)),
		                 mark("score", output::kYellow)},
		                {OutOf(theirs.progress, title.total_units), mark("progress", nullptr)},
		                {DayText(theirs.start_date), mark("started", nullptr)},
		                {DayText(theirs.finish_date), mark("finished", nullptr)},
		                {theirs.hidden ? "hidden" : ""}});
	}
	ctx.out << output::Paint(title.title, output::kBold, ctx.color) << "\n";
	ctx.out << output::Table({{"SITE"},
	                          {"ID"},
	                          {"STATUS", false, true},
	                          {"SCORE"},
	                          {"PROGRESS"},
	                          {"STARTED"},
	                          {"FINISHED"},
	                          {"HIDDEN"}},
	                         rows, ctx.columns, ctx.color);
	for (const auto& line : lines)
		if (not line.differs.empty())
			ctx.err << std::format(
			    "'catalist track --pull {} {}' takes {}'s as the title's; "
			    "'catalist sync' sends yours there once it changes.\n",
			    ProviderKey(line.site), refs::Of(title), ProviderName(line.site));
}

// The sheet into a pipe, a line a site.
void WriteSheet(CliContext& ctx, const Entry& title, const std::vector<SiteLine>& lines,
                const args::Parsed& line) {
	std::vector<std::vector<output::Cell>> rows;
	for (const auto& each : lines) {
		bool listed = each.state == "listed";
		const web::SiteEntry& theirs = each.theirs;
		std::string differ;
		for (const auto& field : each.differs)
			differ += (differ.empty() ? "" : ",") + field;
		auto number = [listed](int value) {
			return listed ? output::Number(value) : output::Cell{"-", output::JsonForm::kNull};
		};
		rows.push_back(
		    {output::Text(ProviderKey(each.site)), output::Known(each.id), output::Text(each.state),
		     listed ? output::Text(StatusKey(theirs.status)) : output::Maybe(""),
		     number(theirs.score), number(theirs.progress), number(theirs.volumes),
		     number(theirs.rewatched), listed ? output::Date(theirs.start_date) : output::Maybe(""),
		     listed ? output::Date(theirs.finish_date) : output::Maybe(""),
		     listed ? output::Cell{theirs.hidden ? "true" : "false", output::JsonForm::kNumber}
		            : output::Maybe(""),
		     output::Maybe(differ), output::Maybe(PageOn(title, each.site))});
	}
	WriteRows(ctx,
	          {"site", "id", "state", "status", "score", "progress", "volumes", "rewatched",
	           "started", "finished", "hidden", "differs", "url"},
	          rows, line.Has("json"), line.Has("header"));
}

// --private SITE and --public SITE: kept, and sent to the site.
int SetPrivacy(const Command& self, const args::Parsed& line, CliContext& ctx, const Entry& title,
               Provider site, bool hidden) {
	if (site == Provider::kMyAnimeList)
		return UsageError(ctx, self, "MyAnimeList has no private entries");
	int id = IdOn(title, site);
	if (id == 0)
		return Fail(ctx, std::format("{} is not linked to {}; link it first: catalist link {} {}",
		                             refs::Of(title), ProviderName(site), refs::Of(title),
		                             ProviderKey(site)));
	if (not ctx.db.SetHidden(site, title.type, title.id, id, hidden))
		return SaveFailed(ctx);
	if (not line.Has("q"))
		ctx.err << std::format("{} ({}) is now {} on {}.\n", title.title, refs::Of(title),
		                       hidden ? "hidden from others" : "shown to others",
		                       ProviderName(site));
	if (accounts::On(ReadAccounts(ctx), site)) {
		Pending told{
		    .site = site, .type = title.type, .id = title.id, .fields = site_field::kHidden};
		if (not ctx.db.Queue({told}))
			ctx.err << "note: could not keep the changes to tell the sites of.\n";
		SendWaiting(ctx);
	}
	return 0;
}

// --pull SITE: your entry there as the title's, one step for undo. The
// other sites are told what changed.
int PullSite(const args::Parsed& line, CliContext& ctx, const Entry& title, Provider site) {
	std::string name = ProviderName(site);
	auto login = accounts::On(ReadAccounts(ctx), site);
	if (IdOn(title, site) == 0 or not login)
		return Fail(ctx, std::format("{} is not {} on {}", refs::Of(title),
		                             login ? "linked" : "something you are logged in to", name));
	auto tracker = TrackerFor(ctx, site);
	auto fresh = CurrentLogin(ctx, *tracker, *login);
	auto yours = fresh ? tracker->EntryOf(title, *fresh)
	                   : web::Answer<std::optional<web::SiteEntry>>(std::unexpected(fresh.error()));
	if (not yours)
		return Fail(ctx, WebError(yours.error()));
	if (not yours->has_value())
		return Fail(ctx, std::format("{} is not on your list on {}", title.title, name));
	auto held = HeldAgain(ctx, title);
	if (not held)
		return 1;
	Entry pulled = PullFromSite(*held, AsTitle(**yours, site), ctx.Today());
	ctx.db.UpdateEntry(pulled);
	journal::Change taken{pulled.type, pulled.id, *held, pulled};
	int code = SaveAndReport(ctx, line, {taken}, false);
	if (code != 0 or journal::ChangedFields(taken).empty())
		return code;
	if (not QueueChanges(ctx, {taken}, site))
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
	SendWaiting(ctx);
	return 0;
}

int RunTrack(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (line.operands.size() != 1)
		return UsageError(ctx, self, "name one title: a ref such as a:12, or part of it");
	int acts =
	    (line.Has("private") ? 1 : 0) + (line.Has("public") ? 1 : 0) + (line.Has("pull") ? 1 : 0);
	if (acts > 1)
		return UsageError(ctx, self, "give one of --private, --public and --pull");
	std::string named = line.Has("private")  ? line.Get("private")
	                    : line.Has("public") ? line.Get("public")
	                                         : line.Get("pull");
	auto site = ParseProvider(named);
	if (acts == 1 and not site)
		return UsageError(ctx, self,
		                  std::format("'{}' is not anilist, mal or kitsu{}", named,
		                              DidYouMean(named, {"anilist", "mal", "kitsu"})));
	if (not Load(ctx))
		return 1;
	auto found = Resolve(ctx, {line.operands[0]}, {OnlyType(line), false, ctx.interactive});
	if (not found)
		return 1;
	const Entry& title = found->front();
	SendWaiting(ctx);
	if (line.Has("private") or line.Has("public"))
		return SetPrivacy(self, line, ctx, title, *site, line.Has("private"));
	if (line.Has("pull"))
		return PullSite(line, ctx, title, *site);

	auto lines = ReadSites(ctx, title);
	if (not line.Has("json") and ctx.columns > 0)
		PrintSheet(ctx, title, lines);
	else
		WriteSheet(ctx, title, lines, line);
	return 0;
}

}  // namespace

std::vector<Command> TrackCommands() {
	std::vector<std::string> sites = {"anilist", "mal", "kitsu"};
	return {
	    {.name = "track",
	     .summary = "Show a title on each site, or hide it or take it from one",
	     .details = std::string(
	                    "Reads your entry of the title on each site you are logged in to where\n"
	                    "it is linked, and shows it beside the lists, what differs in red. Scores\n"
	                    "are five stars at a terminal, a half star a point, whatever scale you\n"
	                    "score in there. Into a pipe each site is one line:\n"
	                    "  site id state status score progress volumes rewatched started\n"
	                    "  finished hidden differs url\n"
	                    "state is listed, missing (not on your list there), unlinked, logged-out\n"
	                    "or failed; score is as the site keeps it (AniList out of 100, Kitsu 2 to\n"
	                    "20, MyAnimeList out of 10).\n\n"
	                    "--private and --public hide the title from others on AniList or Kitsu,\n"
	                    "or show it again. --pull takes your entry on the site as the title's,\n"
	                    "one step for undo, and tells the other sites.\n\n") +
	                kChangeFormat,
	     .options = {{'a', "", "", "look in the anime list only"},
	                 {'m', "", "", "look in the manga list only"},
	                 {0, "private", "SITE", "hide the title from others on the site", sites},
	                 {0, "public", "SITE", "show the title to others on the site again", sites},
	                 {0, "pull", "SITE", "take your entry on the site as the title's", sites},
	                 {0, "json", "", "print one JSON object a line"},
	                 {0, "header", "", "name the columns on the first line of a pipe"},
	                 kQuiet},
	     .operands = "REF",
	     .run = RunTrack,
	     .completes = OperandKind::kRefs},
	};
}

}  // namespace catalist::cli
