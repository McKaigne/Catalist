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
struct siteLine {
	provider site = provider::kAniList;
	int id = 0;
	// listed, missing (not on your list there), unlinked, logged-out or
	// failed.
	std::string state{};
	std::string why{};
	web::siteEntry theirs{};
	// The fields where the site differs from the lists.
	std::vector<std::string> differs{};
};

// The fields where your entry on the site is not as the title has it.
std::vector<std::string> differences(const entry& mine, const web::siteEntry& theirs,
                                     provider site) {
	entry there = asTitle(theirs, site);
	std::vector<std::string> fields;
	auto check = [&fields](bool same, const char* name) {
		if (not same)
			fields.emplace_back(name);
	};
	check(there.status == mine.status, "status");
	check(there.score == mine.score, "score");
	check(there.progress == mine.progress, "progress");
	if (mine.type == mediaType::kManga and site != provider::kKitsu)
		check(there.volumes == mine.volumes, "volumes");
	check(there.rewatched == mine.rewatched, "rewatched");
	check(there.startDate == mine.startDate, "started");
	check(there.finishDate == mine.finishDate, "finished");
	return fields;
}

// Each site's line: read there when the title is linked and you are logged
// in.
std::vector<siteLine> readSites(cliContext& ctx, const entry& title) {
	auto logins = readAccounts(ctx);
	std::vector<siteLine> lines;
	for (provider site : allSites()) {
		siteLine line{site, idOn(title, site)};
		auto login = accounts::on(logins, site);
		if (line.id == 0) {
			line.state = "unlinked";
		} else if (not login) {
			line.state = "logged-out";
		} else {
			auto tracker = trackerFor(ctx, site);
			auto fresh = tracker->current(*login);
			auto yours = fresh ? tracker->entryOf(title, *fresh)
			                   : web::answer<std::optional<web::siteEntry>>(
			                         std::unexpected(fresh.error()));
			if (not yours) {
				line.state = "failed";
				line.why = webError(yours.error());
			} else if (not yours->has_value()) {
				line.state = "missing";
			} else {
				line.state = "listed";
				line.theirs = **yours;
				line.differs = differences(title, line.theirs, site);
			}
		}
		lines.push_back(line);
	}
	return lines;
}

bool differs(const siteLine& line, const std::string& field) {
	return std::find(line.differs.begin(), line.differs.end(), field) != line.differs.end();
}

std::string dayText(const std::optional<isoDate>& day) {
	return day ? day->toString() : "-";
}

// The sheet at a terminal: the lists' line, then a line a site, what
// differs from the lists in red.
void printSheet(cliContext& ctx, const entry& title, const std::vector<siteLine>& lines) {
	std::vector<std::vector<output::styled>> rows;
	rows.push_back({{"Catalist", output::kBold},
	                {refs::of(title), output::kCyan},
	                {statusToString(title.status, title.type)},
	                {output::stars(title.score), output::kYellow},
	                {outOf(title.progress, title.totalUnits)},
	                {dayText(title.startDate)},
	                {dayText(title.finishDate)},
	                {""}});
	for (const auto& line : lines) {
		std::string name = providerName(line.site);
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
			return differs(line, field) ? output::kRed : otherwise;
		};
		const web::siteEntry& theirs = line.theirs;
		rows.push_back(
		    {{name, output::kBold},
		     {id},
		     {statusToString(theirs.status, title.type), mark("status", nullptr)},
		     {output::stars(web::outOfTen(line.site, theirs.score)), mark("score", output::kYellow)},
		     {outOf(theirs.progress, title.totalUnits), mark("progress", nullptr)},
		     {dayText(theirs.startDate), mark("started", nullptr)},
		     {dayText(theirs.finishDate), mark("finished", nullptr)},
		     {theirs.hidden ? "hidden" : ""}});
	}
	ctx.out << output::paint(title.title, output::kBold, ctx.color) << "\n";
	ctx.out << output::table({{"SITE"}, {"ID"}, {"STATUS", false, true}, {"SCORE"},
	                          {"PROGRESS"}, {"STARTED"}, {"FINISHED"}, {"HIDDEN"}},
	                         rows, ctx.columns, ctx.color);
	for (const auto& line : lines)
		if (not line.differs.empty())
			ctx.err << std::format("'catalist track --pull {} {}' takes {}'s as the title's; "
			                       "'catalist sync' sends yours there once it changes.\n",
			                       providerKey(line.site), refs::of(title),
			                       providerName(line.site));
}

// The sheet into a pipe, a line a site.
void writeSheet(cliContext& ctx, const entry& title, const std::vector<siteLine>& lines,
                const args::parsed& line) {
	std::vector<std::vector<output::cell>> rows;
	for (const auto& each : lines) {
		bool listed = each.state == "listed";
		const web::siteEntry& theirs = each.theirs;
		std::string differ;
		for (const auto& field : each.differs)
			differ += (differ.empty() ? "" : ",") + field;
		auto number = [listed](int value) {
			return listed ? output::number(value) : output::cell{"-", output::jsonForm::kNull};
		};
		rows.push_back({output::text(providerKey(each.site)), output::known(each.id),
		                output::text(each.state),
		                listed ? output::text(statusKey(theirs.status)) : output::maybe(""),
		                number(theirs.score), number(theirs.progress), number(theirs.volumes),
		                number(theirs.rewatched),
		                listed ? output::date(theirs.startDate) : output::maybe(""),
		                listed ? output::date(theirs.finishDate) : output::maybe(""),
		                listed ? output::cell{theirs.hidden ? "true" : "false",
		                                      output::jsonForm::kNumber}
		                       : output::maybe(""),
		                output::maybe(differ), output::maybe(pageOn(title, each.site))});
	}
	writeRows(ctx,
	          {"site", "id", "state", "status", "score", "progress", "volumes", "rewatched",
	           "started", "finished", "hidden", "differs", "url"},
	          rows, line.has("json"), line.has("header"));
}

// --private SITE and --public SITE: kept, and sent to the site.
int setPrivacy(const command& self, const args::parsed& line, cliContext& ctx, const entry& title,
               provider site, bool hidden) {
	if (site == provider::kMyAnimeList)
		return usageError(ctx, self, "MyAnimeList has no private entries");
	int id = idOn(title, site);
	if (id == 0)
		return fail(ctx, std::format("{} is not linked to {}; link it first: catalist link {} {}",
		                             refs::of(title), providerName(site), refs::of(title),
		                             providerKey(site)));
	if (not ctx.db.setHidden(site, title.type, title.id, id, hidden))
		return saveFailed(ctx);
	if (not line.has("q"))
		ctx.err << std::format("{} ({}) is now {} on {}.\n", title.title, refs::of(title),
		                       hidden ? "hidden from others" : "shown to others",
		                       providerName(site));
	if (accounts::on(readAccounts(ctx), site)) {
		pending told{.site = site, .type = title.type, .id = title.id,
		             .fields = siteField::kHidden};
		if (not ctx.db.queue({told}))
			ctx.err << "note: could not keep the changes to tell the sites of.\n";
		sendWaiting(ctx);
	}
	return 0;
}

// --pull SITE: your entry there as the title's, one step for undo. The
// other sites are told what changed.
int pullSite(const args::parsed& line, cliContext& ctx, const entry& title, provider site) {
	std::string name = providerName(site);
	auto login = accounts::on(readAccounts(ctx), site);
	if (idOn(title, site) == 0 or not login)
		return fail(ctx, std::format("{} is not {} on {}", refs::of(title),
		                             login ? "linked" : "something you are logged in to", name));
	auto tracker = trackerFor(ctx, site);
	auto fresh = tracker->current(*login);
	auto yours = fresh ? tracker->entryOf(title, *fresh)
	                   : web::answer<std::optional<web::siteEntry>>(std::unexpected(fresh.error()));
	if (not yours)
		return fail(ctx, webError(yours.error()));
	if (not yours->has_value())
		return fail(ctx, std::format("{} is not on your list on {}", title.title, name));
	auto held = heldAgain(ctx, title);
	if (not held)
		return 1;
	entry pulled = pullFromSite(*held, asTitle(**yours, site), ctx.today());
	ctx.db.updateEntry(pulled);
	journal::change taken{pulled.type, pulled.id, *held, pulled};
	int code = saveAndReport(ctx, line, {taken}, false);
	if (code != 0 or journal::changedFields(taken).empty())
		return code;
	if (not queueChanges(ctx, {taken}, site))
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
	sendWaiting(ctx);
	return 0;
}

int runTrack(const command& self, const args::parsed& line, cliContext& ctx) {
	if (line.operands.size() != 1)
		return usageError(ctx, self, "name one title: a ref such as a:12, or part of it");
	int acts = (line.has("private") ? 1 : 0) + (line.has("public") ? 1 : 0) +
	           (line.has("pull") ? 1 : 0);
	if (acts > 1)
		return usageError(ctx, self, "give one of --private, --public and --pull");
	std::string named = line.has("private") ? line.get("private")
	                    : line.has("public")  ? line.get("public")
	                                          : line.get("pull");
	auto site = parseProvider(named);
	if (acts == 1 and not site)
		return usageError(ctx, self,
		                  std::format("'{}' is not anilist, mal or kitsu{}", named,
		                              didYouMean(named, {"anilist", "mal", "kitsu"})));
	if (not load(ctx))
		return 1;
	auto found = resolve(ctx, {line.operands[0]}, {onlyType(line), false, ctx.interactive});
	if (not found)
		return 1;
	const entry& title = found->front();
	sendWaiting(ctx);
	if (line.has("private") or line.has("public"))
		return setPrivacy(self, line, ctx, title, *site, line.has("private"));
	if (line.has("pull"))
		return pullSite(line, ctx, title, *site);

	auto lines = readSites(ctx, title);
	if (not line.has("json") and ctx.columns > 0)
		printSheet(ctx, title, lines);
	else
		writeSheet(ctx, title, lines, line);
	return 0;
}

}  // namespace

std::vector<command> trackCommands() {
	std::vector<std::string> sites = {"anilist", "mal", "kitsu"};
	return {
	    {.name = "track",
	     .summary = "Show a title on each site, or hide it or take it from one",
	     .details =
	         std::string(
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
	     .run = runTrack,
	     .completes = operandKind::kRefs},
	};
}

}  // namespace catalist::cli
