#include <algorithm>
#include <cmath>
#include <format>
#include <ostream>

#include "cli/common.hpp"
#include "cli/refs.hpp"
#include "storage/accounts.hpp"

// Telling the sites, one way, as Mihon tells its trackers: the lists are the
// truth, and each logged-in site where a title is linked is told what
// changed. What cannot be told waits in the outbox until it can.
namespace catalist::cli {

namespace {

// More than this waits for `sync`, not to keep a command waiting on the
// network: an import, say.
constexpr size_t kSendAtOnce = 10;

// A title a site has said no to this many times is given up on, as Mihon's
// delayed updates are after three tries.
constexpr int kMostAttempts = 3;

// How long a title that could not be sent waits before it is tried again:
// five minutes after the first failure, then twice as long each time, up to
// six hours.
long long Backoff(int attempts) {
	long long wait = 300;
	for (int i = 1; i < attempts and wait < 6 * 3600; ++i)
		wait *= 2;
	return std::min<long long>(wait, 6 * 3600);
}

// The fields a change changed that the sites keep (site_field bits): none
// for a change only to the title or the total. Progress that went up is
// only to go up on the sites too.
unsigned FieldsOf(const journal::Change& made) {
	static const std::vector<std::pair<std::string, unsigned>> kKept = {
	    {"added", site_field::kAll},       {"status", site_field::kStatus},
	    {"score", site_field::kScore},     {"progress", site_field::kProgress},
	    {"volumes", site_field::kVolumes}, {"rewatched", site_field::kRewatched},
	    {"started", site_field::kStarted}, {"finished", site_field::kFinished}};
	unsigned fields = 0;
	for (const auto& field : journal::ChangedFields(made))
		for (const auto& [name, bits] : kKept)
			if (field.field == name)
				fields |= bits;
	bool up = made.before and made.after and made.after->progress > made.before->progress;
	if ((fields & site_field::kProgress) and up)
		fields |= site_field::kRaiseOnly;
	return fields;
}

std::string RefOf(const Pending& waiting) {
	return refs::Of(waiting.type, waiting.id);
}

// What telling one site did: how many titles it was told of, how many still
// wait, and the login when its token was renewed on the way.
struct SiteReport {
	size_t told = 0;
	size_t left = 0;
	size_t gave_up = 0;
	std::optional<Account> renewed;
};

// What to send of a title that waits. Only some fields: the site's entry is
// read first, as Mihon refreshes before it updates, so that a site without
// the title gets all of it, and a site further along keeps its progress when
// the progress was only to go up. Fails as the read does.
web::Answer<web::Sending> WhatToSend(web::Tracker& site, const Entry& now, const Pending& each,
                                     const Account& login, CliContext& ctx) {
	web::Sending what{each.fields & ~site_field::kRaiseOnly, false};
	if ((each.fields & site_field::kAll) == site_field::kAll)
		return what;
	auto theirs = site.EntryOf(now, login);
	if (not theirs)
		return std::unexpected(theirs.error());
	ctx.net.pause();
	if (not theirs->has_value())
		what.fields |= site_field::kAll;
	else if ((each.fields & site_field::kRaiseOnly) and (*theirs)->progress >= now.progress)
		what.fields &= ~site_field::kProgress;
	return what;
}

// Tells one site what waits for it, in order. A title the site says no to
// waits, or after kMostAttempts tries is given up; when the site cannot be
// reached, or does not take the login, the rest wait too.
SiteReport SendTo(CliContext& ctx, Provider site, const std::vector<Pending>& mine,
                  const std::optional<Account>& login) {
	SiteReport report;
	// Logged out since: nobody to tell.
	if (not login) {
		for (const auto& each : mine)
			(void)ctx.db.Sent(each);
		return report;
	}
	auto tracker = TrackerFor(ctx, site);
	auto fresh = tracker->Current(*login);
	if (not fresh) {
		ctx.err << "note: " << fresh.error().message << ".\n";
		report.left = mine.size();
		return report;
	}
	if (fresh->token != login->token)
		report.renewed = *fresh;
	for (size_t i = 0; i < mine.size(); ++i) {
		const Pending& each = mine[i];
		if (i > 0)
			ctx.net.pause();
		web::Answer<void> sent;
		if (each.removed) {
			sent = tracker->Remove(each.type, each.site_id, *fresh);
		} else if (auto now = ctx.db.GetById(each.type, each.id)) {
			auto what = WhatToSend(*tracker, *now, each, *fresh, ctx);
			// Hidden or not, as the person said (track --private), with
			// whatever else is sent.
			auto hidden = ctx.db.HiddenOn(site, each.type, each.id, IdOn(*now, site));
			if (what and hidden and (what->fields != 0 or (each.fields & site_field::kHidden))) {
				what->fields |= site_field::kHidden;
				what->hidden = *hidden;
			}
			if (what and what->fields == 0) {
				// The site has all of it already.
				(void)ctx.db.Sent(each);
				continue;
			}
			sent = what ? tracker->Push(*now, *fresh, *what) : std::unexpected(what.error());
		} else {
			// Removed from the lists since, and not from the site.
			(void)ctx.db.Sent(each);
			continue;
		}
		if (sent or sent.error().why == web::Failure::Kind::kNotLinked) {
			(void)ctx.db.Sent(each);
			report.told += sent ? 1 : 0;
			continue;
		}
		int attempts = each.attempts + 1;
		bool refused = sent.error().why == web::Failure::Kind::kRefused;
		if (refused and attempts >= kMostAttempts) {
			ctx.err << std::format("note: gave up telling {} of {} after {} tries: {}.\n",
			                       ProviderName(site), RefOf(each), attempts, sent.error().message);
			(void)ctx.db.Sent(each);
			++report.gave_up;
			continue;
		}
		ctx.err << std::format("note: could not tell {} of {}: {}.\n", ProviderName(site),
		                       RefOf(each), sent.error().message);
		(void)ctx.db.Deferred(each, attempts, ctx.net.now() + Backoff(attempts));
		// The site said no to this title only: it waits, and the rest go on.
		if (refused) {
			++report.left;
			continue;
		}
		report.left += mine.size() - i;
		break;
	}
	return report;
}

}  // namespace

SendReport SendOutbox(CliContext& ctx, bool everything) {
	SendReport report;
	auto waiting = ctx.db.Outbox();
	if (waiting.empty())
		return report;
	// Not yet due: tried a while ago. sync tries everything.
	std::vector<Pending> due;
	long long now = ctx.net.now();
	std::copy_if(
	    waiting.begin(), waiting.end(), std::back_inserter(due),
	    [everything, now](const Pending& each) { return everything or each.next_try <= now; });
	report.left = waiting.size() - due.size();
	if (due.empty())
		return report;
	if (ctx.offline or (not everything and due.size() > kSendAtOnce)) {
		report.left = waiting.size();
		return report;
	}
	auto logins = ReadAccounts(ctx);
	bool renewed = false;
	for (Provider site : AllSites()) {
		std::vector<Pending> mine;
		std::copy_if(due.begin(), due.end(), std::back_inserter(mine),
		             [site](const Pending& each) { return each.site == site; });
		if (mine.empty())
			continue;
		auto sent = SendTo(ctx, site, mine, accounts::On(logins, site));
		report.told += sent.told;
		report.left += sent.left;
		report.gave_up += sent.gave_up;
		if (sent.renewed) {
			logins = accounts::With(logins, *sent.renewed);
			renewed = true;
		}
	}
	// A renewed token is kept for next time.
	if (renewed and not SaveAccounts(ctx, logins))
		ctx.err << "note: could not save the renewed login.\n";
	return report;
}

void NoteWaiting(CliContext& ctx, const SendReport& report) {
	if (report.left == 0 or ctx.offline)
		return;
	ctx.err << std::format(
	    "note: {} to be sent to the sites; 'catalist sync' sends {}.\n",
	    report.left == 1 ? "1 change waits" : std::format("{} changes wait", report.left),
	    report.left == 1 ? "it" : "them");
}

void SendWaiting(CliContext& ctx) {
	if (ctx.offline or ReadAccounts(ctx).empty() or ctx.db.Outbox().empty())
		return;
	NoteWaiting(ctx, SendOutbox(ctx, false));
}

bool QueueChanges(CliContext& ctx, const std::vector<journal::Change>& changes,
                  std::optional<Provider> skip) {
	auto logins = ReadAccounts(ctx);
	std::vector<Pending> more;
	for (const auto& made : changes) {
		unsigned fields = FieldsOf(made);
		if (not made.after or fields == 0)
			continue;
		for (const auto& login : logins)
			if (login.site != skip and IdOn(*made.after, login.site) > 0)
				more.push_back(
				    {.site = login.site, .type = made.type, .id = made.id, .fields = fields});
	}
	return more.empty() or ctx.db.Queue(more);
}

void TellSites(CliContext& ctx, const std::vector<journal::Change>& changes) {
	if (ReadAccounts(ctx).empty())
		return;
	if (not QueueChanges(ctx, changes, std::nullopt)) {
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
		return;
	}
	SendWaiting(ctx);
}

void TellSitesGone(CliContext& ctx, const std::vector<Entry>& gone) {
	std::vector<Pending> more;
	for (const auto& login : ReadAccounts(ctx))
		for (const auto& title : gone)
			if (int site_id = IdOn(title, login.site); site_id > 0)
				more.push_back({login.site, title.type, title.id, true, site_id});
	if (more.empty())
		return;
	if (not ctx.db.Queue(more)) {
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
		return;
	}
	NoteWaiting(ctx, SendOutbox(ctx, false));
}

namespace {

// "status, progress (up only)", "everything" or "remove": what a title that
// waits is to tell its site.
std::string ChangeOf(const Pending& waiting) {
	if (waiting.removed)
		return "remove";
	if ((waiting.fields & site_field::kAll) == site_field::kAll)
		return "everything";
	static const std::vector<std::pair<unsigned, std::string>> kNames = {
	    {site_field::kStatus, "status"},       {site_field::kScore, "score"},
	    {site_field::kProgress, "progress"},   {site_field::kVolumes, "volumes"},
	    {site_field::kRewatched, "rewatched"}, {site_field::kStarted, "started"},
	    {site_field::kFinished, "finished"},   {site_field::kHidden, "hidden"}};
	std::string named;
	for (const auto& [bit, name] : kNames) {
		if (not(waiting.fields & bit))
			continue;
		named += (named.empty() ? "" : ", ") + name;
		if (bit == site_field::kProgress and (waiting.fields & site_field::kRaiseOnly))
			named += " (up only)";
	}
	return named;
}

// "now", or "in 10 min" while a title that failed waits to be tried again.
std::string DueText(long long next_try, long long now) {
	if (next_try <= now)
		return "now";
	long long minutes = (next_try - now + 59) / 60;
	return minutes < 120 ? std::format("in {} min", minutes)
	                     : std::format("in {} h", (minutes + 59) / 60);
}

// sync --list: what waits, for which site, what it is to tell, and when.
int ListWaiting(const args::Parsed& line, CliContext& ctx) {
	auto waiting = ctx.db.Outbox();
	long long now = ctx.net.now();
	if (not line.Has("json") and ctx.columns > 0) {
		if (waiting.empty()) {
			ctx.err << "Nothing waits to be sent.\n";
			return 0;
		}
		std::vector<std::vector<output::Styled>> rows;
		for (const auto& each : waiting)
			rows.push_back({{RefOf(each), output::kCyan},
			                {ProviderName(each.site)},
			                {ChangeOf(each), nullptr},
			                {std::to_string(each.attempts)},
			                {DueText(each.next_try, now)}});
		ctx.out << output::Table(
		    {{"REF"}, {"SITE"}, {"CHANGE", false, true}, {"TRIES", true}, {"NEXT"}}, rows,
		    ctx.columns, ctx.color);
		return 0;
	}
	std::vector<std::vector<output::Cell>> rows;
	for (const auto& each : waiting)
		rows.push_back({output::Text(RefOf(each)), output::Text(ProviderKey(each.site)),
		                output::Text(ChangeOf(each)), output::Number(each.attempts),
		                output::Number(std::max(0LL, each.next_try - now))});
	WriteRows(ctx, {"ref", "site", "change", "tries", "due_in"}, rows, line.Has("json"),
	          line.Has("header"));
	return 0;
}

int RunSync(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (not line.operands.empty())
		return UsageError(ctx, self, "sync takes no operands");
	int modes =
	    (line.Has("all") ? 1 : 0) + (line.Has("forget") ? 1 : 0) + (line.Has("list") ? 1 : 0);
	if (modes > 1)
		return UsageError(ctx, self, "give one of --all, --forget and --list");
	if (not Load(ctx))
		return 1;
	if (line.Has("list"))
		return ListWaiting(line, ctx);
	auto waiting = ctx.db.Outbox();
	if (line.Has("forget")) {
		for (const auto& each : waiting)
			(void)ctx.db.Sent(each);
		ctx.err << std::format(
		    "Forgot {} the sites were not told of.\n",
		    waiting.size() == 1 ? "1 change" : std::format("{} changes", waiting.size()));
		return 0;
	}
	auto logins = ReadAccounts(ctx);
	if (logins.empty()) {
		ctx.err << "You are not logged in to any site. Log in with: catalist login SITE\n";
		return waiting.empty() ? 0 : 1;
	}
	if (line.Has("all")) {
		std::vector<Pending> every;
		for (const auto& login : logins)
			for (MediaType type : {MediaType::kAnime, MediaType::kManga})
				for (const auto& title : ctx.db.GetEntries(type))
					if (IdOn(title, login.site) > 0)
						every.push_back({login.site, type, title.id});
		if (not ctx.db.Queue(every))
			return Fail(ctx, "could not keep the changes to tell the sites of");
		if (not line.Has("q") and every.size() > 1)
			ctx.err << std::format(
			    "Telling the sites of {}; MyAnimeList and Kitsu take a second "
			    "each.\n",
			    Titles(every.size()));
	}
	if (ctx.offline)
		return Fail(ctx, "--offline: nothing is sent");
	auto report = SendOutbox(ctx, true);
	if (not line.Has("q")) {
		if (report.told == 0 and report.left == 0 and report.gave_up == 0)
			ctx.err << "Nothing to send.\n";
		else if (report.told > 0)
			ctx.err << std::format(
			    "Told the sites of {}.\n",
			    report.told == 1 ? "1 change" : std::format("{} changes", report.told));
	}
	if (report.left > 0)
		return Fail(ctx, std::format("{} still to be sent; try again later",
		                             report.left == 1 ? "1 change"
		                                              : std::format("{} changes", report.left)));
	return report.gave_up > 0 ? 1 : 0;
}

// The REF and the site of link and unlink, the title resolved. Nothing,
// with the exit code, when there is nothing to go on with.
struct LinkRequest {
	std::optional<Entry> title;
	Provider site = Provider::kAniList;
	int code = 0;
};

LinkRequest ReadLink(const Command& self, const args::Parsed& line, CliContext& ctx, size_t most) {
	LinkRequest request;
	if (line.operands.size() < 2 or line.operands.size() > most) {
		request.code = UsageError(ctx, self, std::format("give {}", self.operands));
		return request;
	}
	auto site = ParseProvider(line.operands[1]);
	if (not site) {
		request.code = UsageError(
		    ctx, self,
		    std::format("unknown site '{}'; choose anilist, mal or kitsu", line.operands[1]));
		return request;
	}
	request.site = *site;
	// Only read: link may ask the site first, and no other program is to
	// wait on that.
	if (not Load(ctx)) {
		request.code = 1;
		return request;
	}
	auto found = Resolve(ctx, {line.operands[0]}, {OnlyType(line), false, ctx.interactive});
	if (not found or found->size() != 1) {
		request.code = 1;
		return request;
	}
	request.title = found->front();
	return request;
}

Entry WithSiteId(Entry title, Provider site, int id) {
	switch (site) {
		case Provider::kAniList:
			title.anilist_id = id;
			break;
		case Provider::kMyAnimeList:
			title.mal_id = id;
			break;
		case Provider::kKitsu:
			title.kitsu_id = id;
			break;
	}
	return title;
}

// The ids link gives the title on `site`: the ID given, else those of the
// match chosen of a search (for --query's text, or the title's), the person
// choosing as for add. Nothing, after the reason, when there is none.
std::optional<Entry> ChosenIds(const Command& self, const args::Parsed& line, CliContext& ctx,
                               const Entry& title, Provider site, int& code) {
	std::string name = ProviderName(site);
	Entry chosen;
	code = 1;
	if (line.operands.size() == 3) {
		auto id = args::ParseCount(line.operands[2]);
		if (not id or *id == 0) {
			code = UsageError(ctx, self,
			                  std::format("'{}' is not an id on {}", line.operands[2], name));
			return std::nullopt;
		}
		return WithSiteId(chosen, site, *id);
	}
	std::string query = line.Has("query") ? line.Get("query") : title.title;
	auto tracker = TrackerFor(ctx, site);
	auto match = ChooseMatch(ctx, *tracker, title.type, query, std::nullopt);
	if (not match)
		return std::nullopt;
	chosen.anilist_id = match->anilist_id;
	chosen.mal_id = match->mal_id;
	chosen.kitsu_id = match->kitsu_id;
	return chosen;
}

// The title with the site's id, and the other ids the match knows that it
// lacks.
Entry WithIds(Entry title, const Entry& chosen, Provider site) {
	for (Provider other : AllSites())
		if (IdOn(title, other) == 0)
			title = WithSiteId(title, other, IdOn(chosen, other));
	return WithSiteId(title, site, IdOn(chosen, site));
}

// Links a title as Mihon binds a tracker: logged in to the site, your entry
// there is read first and folded into the title (MergeLinked: one step for
// undo), then the title is sent there whole.
int RunLink(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto request = ReadLink(self, line, ctx, 3);
	if (not request.title)
		return request.code;
	if (line.operands.size() == 3 and line.Has("query"))
		return UsageError(ctx, self, "give an ID or --query, not both");
	Provider site = request.site;
	std::string name = ProviderName(site);
	int code = 0;
	auto chosen = ChosenIds(self, line, ctx, *request.title, site, code);
	if (not chosen)
		return code;

	// Your entry there, read before the lists are taken for writing.
	auto login = accounts::On(ReadAccounts(ctx), site);
	// Read, and there; read, and not on your list; or not read.
	bool read = false;
	std::optional<web::SiteEntry> theirs;
	if (login and not ctx.offline) {
		auto tracker = TrackerFor(ctx, site);
		auto fresh = tracker->Current(*login);
		auto yours =
		    fresh ? tracker->EntryOf(WithIds(*request.title, *chosen, site), *fresh)
		          : web::Answer<std::optional<web::SiteEntry>>(std::unexpected(fresh.error()));
		if (not yours) {
			Fail(ctx, WebError(yours.error()));
			ctx.err << "Nothing was linked. With --offline the title is linked without reading "
			           "your entry there, and sent whole later.\n";
			return 1;
		}
		read = true;
		theirs = *yours;
	}

	auto held = HeldAgain(ctx, *request.title);
	if (not held)
		return 1;
	Entry title = WithIds(*held, *chosen, site);
	int id = IdOn(title, site);
	for (const auto& other : ctx.db.GetEntries(title.type))
		if (other.id != title.id and IdOn(other, site) == id)
			return Fail(ctx, std::format("{} is {} {} already", refs::Of(other), name, id));
	Entry merged = theirs ? MergeLinked(title, AsTitle(*theirs, site), ctx.Today()) : title;
	ctx.db.UpdateEntry(merged);
	if (not line.Has("q"))
		ctx.err << std::format("Linked {} ({}) to {} {}{}.\n", merged.title, refs::Of(merged), name,
		                       id,
		                       not login  ? ""
		                       : not read ? "; your entry there was not read (--offline)"
		                       : theirs   ? "; your entry there is folded in"
		                                  : "; it is new on your list there");
	// What your entry there brought is a step for undo; an id alone is not.
	journal::Change folded{merged.type, merged.id, title, merged};
	int saved = journal::ChangedFields(folded).empty() ? (ctx.db.SaveAll() ? 0 : SaveFailed(ctx))
	                                                   : SaveAndReport(ctx, line, {folded}, false);
	if (saved != 0 or not login)
		return saved;
	// The site gets all of it; the other sites what the fold changed.
	bool kept = ctx.db.Queue({{.site = site, .type = merged.type, .id = merged.id}}) and
	            QueueChanges(ctx, {folded}, std::nullopt);
	if (not kept)
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
	SendWaiting(ctx);
	return 0;
}

int RunUnlink(const Command& self, const args::Parsed& line, CliContext& ctx) {
	auto request = ReadLink(self, line, ctx, 2);
	if (not request.title)
		return request.code;
	std::string name = ProviderName(request.site);
	auto title = HeldAgain(ctx, *request.title);
	if (not title)
		return 1;
	if (IdOn(*title, request.site) == 0) {
		ctx.err << std::format("{} is not linked to {}.\n", refs::Of(*title), name);
		return 0;
	}
	int site_id = IdOn(*title, request.site);
	ctx.db.UpdateEntry(WithSiteId(*title, request.site, 0));
	if (not ctx.db.SaveAll())
		return SaveFailed(ctx);
	bool remote = line.Has("remote");
	if (not line.Has("q"))
		ctx.err << std::format(
		    "Unlinked {} ({}) from {}{}.\n", title->title, refs::Of(*title), name,
		    remote ? "; it comes off your list there" : ": it stays on your list there");
	if (not remote)
		return 0;
	if (not accounts::On(ReadAccounts(ctx), request.site))
		return Fail(ctx, std::format("you are not logged in to {}: log in to take it off "
		                             "your list there",
		                             name));
	Pending gone{.site = request.site,
	             .type = title->type,
	             .id = title->id,
	             .removed = true,
	             .site_id = site_id};
	if (not ctx.db.Queue({gone}))
		return Fail(ctx, "could not keep the changes to tell the sites of");
	SendWaiting(ctx);
	return 0;
}

}  // namespace

std::vector<Command> SyncCommands() {
	return {
	    {.name = "sync",
	     .summary = "Tell the sites what they have not heard of yet",
	     .details = "Each change to a title (its status, score, progress, volumes, rewatches\n"
	                "or dates) is sent at once to every site you are logged in to where the\n"
	                "title is linked, as Mihon tells its trackers. Only the fields that changed\n"
	                "are sent, so what you changed on the site itself stays, and progress that\n"
	                "went up does not lower a site that is further along. What could not be\n"
	                "sent (offline, a site down, a login run out, more than ten changes at\n"
	                "once) waits: the next command that goes online sends it once it is due,\n"
	                "five minutes after a failure and twice as long after each one more, and\n"
	                "sync sends it at once. A change a site refuses three times is given up.\n"
	                "With --all every linked title is sent whole, as after a first login.\n\n"
	                "--list prints what waits; into a pipe one line each: ref, site, change,\n"
	                "tries and due_in (seconds).\n",
	     .options = {{0, "all", "", "send every linked title, not only what waits"},
	                 {0, "forget", "", "drop what waits, telling the sites nothing"},
	                 {0, "list", "", "print what waits, and when it is tried again"},
	                 {0, "json", "", "with --list: one JSON object a line"},
	                 {0, "header", "", "with --list: name the columns on the first line"},
	                 kQuiet},
	     .run = RunSync},
	    {.name = "link",
	     .summary = "Link a title to its entry on a site",
	     .details =
	         std::string(
	             "Sets the title's id on the site: the ID given, or the match you choose of\n"
	             "a search for its title there (or for --query's TEXT). Logged in to the\n"
	             "site, your entry there is read first, as Mihon does: what you set here\n"
	             "stays, and the site fills in the rest (the higher progress, a score or\n"
	             "dates you have not set, its status while the title is only planned\n"
	             "here). What that changes is one step for undo; the title is then sent\n"
	             "there whole, and changes after it as they come (see 'sync'). Titles\n"
	             "added from a site, imported or refreshed are linked to every site that\n"
	             "knew their ids.\n\n") +
	         kChangeFormat,
	     .options = {{'a', "", "", "look in the anime list only"},
	                 {'m', "", "", "look in the manga list only"},
	                 {0, "query", "TEXT", "search the site for TEXT, not the title"},
	                 kQuiet},
	     .operands = "REF anilist|mal|kitsu [ID]",
	     .run = RunLink,
	     .completes = OperandKind::kRefs},
	    {.name = "unlink",
	     .summary = "Stop telling a site of a title",
	     .details = "Forgets the title's id on the site, so changes to it are no longer\n"
	                "sent there. The site's entry stays as it is, unless --remote takes it\n"
	                "off your list there too. Not a step for undo.\n",
	     .options = {{'a', "", "", "look in the anime list only"},
	                 {'m', "", "", "look in the manga list only"},
	                 {0, "remote", "", "take it off your list on the site as well"},
	                 kQuiet},
	     .operands = "REF anilist|mal|kitsu",
	     .run = RunUnlink,
	     .completes = OperandKind::kRefs},
	};
}

}  // namespace catalist::cli
