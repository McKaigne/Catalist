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

const std::vector<provider> kSites = {provider::kAniList, provider::kMyAnimeList, provider::kKitsu};

// How long a title that could not be sent waits before it is tried again:
// five minutes after the first failure, then twice as long each time, up to
// six hours.
long long backoff(int attempts) {
	long long wait = 300;
	for (int i = 1; i < attempts and wait < 6 * 3600; ++i)
		wait *= 2;
	return std::min<long long>(wait, 6 * 3600);
}

// The fields a change changed that the sites keep (siteField bits): none
// for a change only to the title or the total. Progress that went up is
// only to go up on the sites too.
unsigned fieldsOf(const journal::change& made) {
	static const std::vector<std::pair<std::string, unsigned>> kKept = {
	    {"added", siteField::kAll},         {"status", siteField::kStatus},
	    {"score", siteField::kScore},       {"progress", siteField::kProgress},
	    {"volumes", siteField::kVolumes},   {"rewatched", siteField::kRewatched},
	    {"started", siteField::kStarted},   {"finished", siteField::kFinished}};
	unsigned fields = 0;
	for (const auto& field : journal::changedFields(made))
		for (const auto& [name, bits] : kKept)
			if (field.field == name)
				fields |= bits;
	bool up = made.before and made.after and made.after->progress > made.before->progress;
	if ((fields & siteField::kProgress) and up)
		fields |= siteField::kRaiseOnly;
	return fields;
}

std::string refOf(const pending& waiting) {
	return refs::of(waiting.type, waiting.id);
}

// What telling one site did: how many titles it was told of, how many still
// wait, and the login when its token was renewed on the way.
struct siteReport {
	size_t told = 0;
	size_t left = 0;
	size_t gaveUp = 0;
	std::optional<account> renewed;
};

// What to send of a title that waits. Only some fields: the site's entry is
// read first, as Mihon refreshes before it updates, so that a site without
// the title gets all of it, and a site further along keeps its progress when
// the progress was only to go up. Fails as the read does.
web::answer<web::sending> whatToSend(web::iTracker& site, const entry& now, const pending& each,
                                     const account& login, cliContext& ctx) {
	web::sending what{each.fields & ~siteField::kRaiseOnly, false};
	if ((each.fields & siteField::kAll) == siteField::kAll)
		return what;
	auto theirs = site.entryOf(now, login);
	if (not theirs)
		return std::unexpected(theirs.error());
	ctx.net.pause();
	if (not theirs->has_value())
		what.fields |= siteField::kAll;
	else if ((each.fields & siteField::kRaiseOnly) and (*theirs)->progress >= now.progress)
		what.fields &= ~siteField::kProgress;
	return what;
}

// Tells one site what waits for it, in order. A title the site says no to
// waits, or after kMostAttempts tries is given up; when the site cannot be
// reached, or does not take the login, the rest wait too.
siteReport sendTo(cliContext& ctx, provider site, const std::vector<pending>& mine,
                  const std::optional<account>& login) {
	siteReport report;
	// Logged out since: nobody to tell.
	if (not login) {
		for (const auto& each : mine)
			(void)ctx.db.sent(each);
		return report;
	}
	auto tracker = trackerFor(ctx, site);
	auto fresh = tracker->current(*login);
	if (not fresh) {
		ctx.err << "note: " << fresh.error().message << ".\n";
		report.left = mine.size();
		return report;
	}
	if (fresh->token != login->token)
		report.renewed = *fresh;
	for (size_t i = 0; i < mine.size(); ++i) {
		const pending& each = mine[i];
		if (i > 0)
			ctx.net.pause();
		web::answer<void> sent;
		if (each.removed) {
			sent = tracker->remove(each.type, each.siteId, *fresh);
		} else if (auto now = ctx.db.getById(each.type, each.id)) {
			auto what = whatToSend(*tracker, *now, each, *fresh, ctx);
			if (what and what->fields == 0) {
				// The site has all of it already.
				(void)ctx.db.sent(each);
				continue;
			}
			sent = what ? tracker->push(*now, *fresh, *what) : std::unexpected(what.error());
		} else {
			// Removed from the lists since, and not from the site.
			(void)ctx.db.sent(each);
			continue;
		}
		if (sent or sent.error().why == web::failure::kind::kNotLinked) {
			(void)ctx.db.sent(each);
			report.told += sent ? 1 : 0;
			continue;
		}
		int attempts = each.attempts + 1;
		bool refused = sent.error().why == web::failure::kind::kRefused;
		if (refused and attempts >= kMostAttempts) {
			ctx.err << std::format("note: gave up telling {} of {} after {} tries: {}.\n",
			                       providerName(site), refOf(each), attempts, sent.error().message);
			(void)ctx.db.sent(each);
			++report.gaveUp;
			continue;
		}
		ctx.err << std::format("note: could not tell {} of {}: {}.\n", providerName(site),
		                       refOf(each), sent.error().message);
		(void)ctx.db.deferred(each, attempts, ctx.net.now() + backoff(attempts));
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

sendReport sendOutbox(cliContext& ctx, bool everything) {
	sendReport report;
	auto waiting = ctx.db.outbox();
	if (waiting.empty())
		return report;
	// Not yet due: tried a while ago. sync tries everything.
	std::vector<pending> due;
	long long now = ctx.net.now();
	std::copy_if(waiting.begin(), waiting.end(), std::back_inserter(due),
	             [everything, now](const pending& each) {
		             return everything or each.nextTry <= now;
	             });
	report.left = waiting.size() - due.size();
	if (due.empty())
		return report;
	if (ctx.offline or (not everything and due.size() > kSendAtOnce)) {
		report.left = waiting.size();
		return report;
	}
	auto logins = readAccounts(ctx);
	bool renewed = false;
	for (provider site : kSites) {
		std::vector<pending> mine;
		std::copy_if(due.begin(), due.end(), std::back_inserter(mine),
		             [site](const pending& each) { return each.site == site; });
		if (mine.empty())
			continue;
		auto sent = sendTo(ctx, site, mine, accounts::on(logins, site));
		report.told += sent.told;
		report.left += sent.left;
		report.gaveUp += sent.gaveUp;
		if (sent.renewed) {
			logins = accounts::with(logins, *sent.renewed);
			renewed = true;
		}
	}
	// A renewed token is kept for next time.
	if (renewed and not saveAccounts(ctx, logins))
		ctx.err << "note: could not save the renewed login.\n";
	return report;
}

void noteWaiting(cliContext& ctx, const sendReport& report) {
	if (report.left == 0 or ctx.offline)
		return;
	ctx.err << std::format(
	    "note: {} to be sent to the sites; 'catalist sync' sends {}.\n",
	    report.left == 1 ? "1 change waits" : std::format("{} changes wait", report.left),
	    report.left == 1 ? "it" : "them");
}

void sendWaiting(cliContext& ctx) {
	if (ctx.offline or readAccounts(ctx).empty() or ctx.db.outbox().empty())
		return;
	noteWaiting(ctx, sendOutbox(ctx, false));
}

bool queueChanges(cliContext& ctx, const std::vector<journal::change>& changes) {
	auto logins = readAccounts(ctx);
	std::vector<pending> more;
	for (const auto& made : changes) {
		unsigned fields = fieldsOf(made);
		if (not made.after or fields == 0)
			continue;
		for (const auto& login : logins)
			if (idOn(*made.after, login.site) > 0)
				more.push_back({.site = login.site,
				                .type = made.type,
				                .id = made.id,
				                .fields = fields});
	}
	return more.empty() or ctx.db.queue(more);
}

void tellSites(cliContext& ctx, const std::vector<journal::change>& changes) {
	if (readAccounts(ctx).empty())
		return;
	if (not queueChanges(ctx, changes)) {
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
		return;
	}
	sendWaiting(ctx);
}

void tellSitesGone(cliContext& ctx, const std::vector<entry>& gone) {
	std::vector<pending> more;
	for (const auto& login : readAccounts(ctx))
		for (const auto& title : gone)
			if (int siteId = idOn(title, login.site); siteId > 0)
				more.push_back({login.site, title.type, title.id, true, siteId});
	if (more.empty())
		return;
	if (not ctx.db.queue(more)) {
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
		return;
	}
	noteWaiting(ctx, sendOutbox(ctx, false));
}

namespace {

// "status, progress (up only)", "everything" or "remove": what a title that
// waits is to tell its site.
std::string changeOf(const pending& waiting) {
	if (waiting.removed)
		return "remove";
	if ((waiting.fields & siteField::kAll) == siteField::kAll)
		return "everything";
	static const std::vector<std::pair<unsigned, std::string>> kNames = {
	    {siteField::kStatus, "status"},     {siteField::kScore, "score"},
	    {siteField::kProgress, "progress"}, {siteField::kVolumes, "volumes"},
	    {siteField::kRewatched, "rewatched"}, {siteField::kStarted, "started"},
	    {siteField::kFinished, "finished"}, {siteField::kHidden, "hidden"}};
	std::string named;
	for (const auto& [bit, name] : kNames) {
		if (not(waiting.fields & bit))
			continue;
		named += (named.empty() ? "" : ", ") + name;
		if (bit == siteField::kProgress and (waiting.fields & siteField::kRaiseOnly))
			named += " (up only)";
	}
	return named;
}

// "now", or "in 10 min" while a title that failed waits to be tried again.
std::string dueText(long long nextTry, long long now) {
	if (nextTry <= now)
		return "now";
	long long minutes = (nextTry - now + 59) / 60;
	return minutes < 120 ? std::format("in {} min", minutes)
	                     : std::format("in {} h", (minutes + 59) / 60);
}

// sync --list: what waits, for which site, what it is to tell, and when.
int listWaiting(const args::parsed& line, cliContext& ctx) {
	auto waiting = ctx.db.outbox();
	long long now = ctx.net.now();
	if (not line.has("json") and ctx.columns > 0) {
		if (waiting.empty()) {
			ctx.err << "Nothing waits to be sent.\n";
			return 0;
		}
		std::vector<std::vector<output::styled>> rows;
		for (const auto& each : waiting)
			rows.push_back({{refOf(each), output::kCyan},
			                {providerName(each.site)},
			                {changeOf(each), nullptr},
			                {std::to_string(each.attempts)},
			                {dueText(each.nextTry, now)}});
		ctx.out << output::table({{"REF"}, {"SITE"}, {"CHANGE", false, true}, {"TRIES", true},
		                          {"NEXT"}},
		                         rows, ctx.columns, ctx.color);
		return 0;
	}
	std::vector<std::vector<output::cell>> rows;
	for (const auto& each : waiting)
		rows.push_back({output::text(refOf(each)), output::text(providerKey(each.site)),
		                output::text(changeOf(each)), output::number(each.attempts),
		                output::number(std::max(0LL, each.nextTry - now))});
	writeRows(ctx, {"ref", "site", "change", "tries", "due_in"}, rows, line.has("json"),
	          line.has("header"));
	return 0;
}

int runSync(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not line.operands.empty())
		return usageError(ctx, self, "sync takes no operands");
	int modes = (line.has("all") ? 1 : 0) + (line.has("forget") ? 1 : 0) + (line.has("list") ? 1 : 0);
	if (modes > 1)
		return usageError(ctx, self, "give one of --all, --forget and --list");
	if (not load(ctx))
		return 1;
	if (line.has("list"))
		return listWaiting(line, ctx);
	auto waiting = ctx.db.outbox();
	if (line.has("forget")) {
		for (const auto& each : waiting)
			(void)ctx.db.sent(each);
		ctx.err << std::format(
		    "Forgot {} the sites were not told of.\n",
		    waiting.size() == 1 ? "1 change" : std::format("{} changes", waiting.size()));
		return 0;
	}
	auto logins = readAccounts(ctx);
	if (logins.empty()) {
		ctx.err << "You are not logged in to any site. Log in with: catalist login SITE\n";
		return waiting.empty() ? 0 : 1;
	}
	if (line.has("all")) {
		std::vector<pending> every;
		for (const auto& login : logins)
			for (mediaType type : {mediaType::kAnime, mediaType::kManga})
				for (const auto& title : ctx.db.getEntries(type))
					if (idOn(title, login.site) > 0)
						every.push_back({login.site, type, title.id});
		if (not ctx.db.queue(every))
			return fail(ctx, "could not keep the changes to tell the sites of");
		if (not line.has("q") and every.size() > 1)
			ctx.err << std::format(
			    "Telling the sites of {}; MyAnimeList and Kitsu take a second "
			    "each.\n",
			    titles(every.size()));
	}
	if (ctx.offline)
		return fail(ctx, "--offline: nothing is sent");
	auto report = sendOutbox(ctx, true);
	if (not line.has("q")) {
		if (report.told == 0 and report.left == 0 and report.gaveUp == 0)
			ctx.err << "Nothing to send.\n";
		else if (report.told > 0)
			ctx.err << std::format(
			    "Told the sites of {}.\n",
			    report.told == 1 ? "1 change" : std::format("{} changes", report.told));
	}
	if (report.left > 0)
		return fail(ctx, std::format("{} still to be sent; try again later",
		                             report.left == 1 ? "1 change"
		                                              : std::format("{} changes", report.left)));
	return report.gaveUp > 0 ? 1 : 0;
}

// The REF and the site of link and unlink, the title resolved. Nothing,
// with the exit code, when there is nothing to go on with.
struct linkRequest {
	std::optional<entry> title;
	provider site = provider::kAniList;
	int code = 0;
};

linkRequest readLink(const command& self, const args::parsed& line, cliContext& ctx, size_t most) {
	linkRequest request;
	if (line.operands.size() < 2 or line.operands.size() > most) {
		request.code = usageError(ctx, self, std::format("give {}", self.operands));
		return request;
	}
	auto site = parseProvider(line.operands[1]);
	if (not site) {
		request.code = usageError(
		    ctx, self,
		    std::format("unknown site '{}'; choose anilist, mal or kitsu", line.operands[1]));
		return request;
	}
	request.site = *site;
	// Only read: link may ask the site first, and no other program is to
	// wait on that.
	if (not load(ctx)) {
		request.code = 1;
		return request;
	}
	auto found = resolve(ctx, {line.operands[0]}, {onlyType(line), false, ctx.interactive});
	if (not found or found->size() != 1) {
		request.code = 1;
		return request;
	}
	request.title = found->front();
	return request;
}

// Takes the catalogue for writing and reads the title again, as it is now.
// Nothing, with the reason on standard error, when it is gone meanwhile.
std::optional<entry> heldAgain(cliContext& ctx, const entry& title) {
	if (not loadForWrite(ctx))
		return std::nullopt;
	auto now = ctx.db.getById(title.type, title.id);
	if (not now)
		fail(ctx, std::format("{} was removed meanwhile", refs::of(title)));
	return now;
}

entry withSiteId(entry title, provider site, int id) {
	switch (site) {
		case provider::kAniList:
			title.anilistId = id;
			break;
		case provider::kMyAnimeList:
			title.malId = id;
			break;
		case provider::kKitsu:
			title.kitsuId = id;
			break;
	}
	return title;
}

// Your entry on a site as a title of the lists, for mergeLinked.
entry asTitle(const web::siteEntry& theirs, provider site) {
	entry title;
	title.status = theirs.status;
	title.score = static_cast<int>(std::lround(web::outOfTen(site, theirs.score)));
	title.progress = theirs.progress;
	title.volumes = theirs.volumes;
	title.rewatched = theirs.rewatched;
	title.startDate = theirs.startDate;
	title.finishDate = theirs.finishDate;
	return title;
}

// The ids link gives the title on `site`: the ID given, else those of the
// match chosen of a search (for --query's text, or the title's), the person
// choosing as for add. Nothing, after the reason, when there is none.
std::optional<entry> chosenIds(const command& self, const args::parsed& line, cliContext& ctx,
                               const entry& title, provider site, int& code) {
	std::string name = providerName(site);
	entry chosen;
	code = 1;
	if (line.operands.size() == 3) {
		auto id = args::parseCount(line.operands[2]);
		if (not id or *id == 0) {
			code = usageError(ctx, self, std::format("'{}' is not an id on {}", line.operands[2], name));
			return std::nullopt;
		}
		return withSiteId(chosen, site, *id);
	}
	std::string query = line.has("query") ? line.get("query") : title.title;
	auto tracker = trackerFor(ctx, site);
	auto match = chooseMatch(ctx, *tracker, title.type, query, std::nullopt);
	if (not match)
		return std::nullopt;
	chosen.anilistId = match->anilistId;
	chosen.malId = match->malId;
	chosen.kitsuId = match->kitsuId;
	return chosen;
}

// The title with the site's id, and the other ids the match knows that it
// lacks.
entry withIds(entry title, const entry& chosen, provider site) {
	for (provider other : kSites)
		if (idOn(title, other) == 0)
			title = withSiteId(title, other, idOn(chosen, other));
	return withSiteId(title, site, idOn(chosen, site));
}

// Links a title as Mihon binds a tracker: logged in to the site, your entry
// there is read first and folded into the title (mergeLinked: one step for
// undo), then the title is sent there whole.
int runLink(const command& self, const args::parsed& line, cliContext& ctx) {
	auto request = readLink(self, line, ctx, 3);
	if (not request.title)
		return request.code;
	if (line.operands.size() == 3 and line.has("query"))
		return usageError(ctx, self, "give an ID or --query, not both");
	provider site = request.site;
	std::string name = providerName(site);
	int code = 0;
	auto chosen = chosenIds(self, line, ctx, *request.title, site, code);
	if (not chosen)
		return code;

	// Your entry there, read before the lists are taken for writing.
	auto login = accounts::on(readAccounts(ctx), site);
	// Read, and there; read, and not on your list; or not read.
	bool read = false;
	std::optional<web::siteEntry> theirs;
	if (login and not ctx.offline) {
		auto tracker = trackerFor(ctx, site);
		auto fresh = tracker->current(*login);
		auto yours = fresh ? tracker->entryOf(withIds(*request.title, *chosen, site), *fresh)
		                   : web::answer<std::optional<web::siteEntry>>(std::unexpected(fresh.error()));
		if (not yours) {
			fail(ctx, webError(yours.error()));
			ctx.err << "Nothing was linked. With --offline the title is linked without reading "
			           "your entry there, and sent whole later.\n";
			return 1;
		}
		read = true;
		theirs = *yours;
	}

	auto held = heldAgain(ctx, *request.title);
	if (not held)
		return 1;
	entry title = withIds(*held, *chosen, site);
	int id = idOn(title, site);
	for (const auto& other : ctx.db.getEntries(title.type))
		if (other.id != title.id and idOn(other, site) == id)
			return fail(ctx, std::format("{} is {} {} already", refs::of(other), name, id));
	entry merged = theirs ? mergeLinked(title, asTitle(*theirs, site), ctx.today()) : title;
	ctx.db.updateEntry(merged);
	if (not line.has("q"))
		ctx.err << std::format("Linked {} ({}) to {} {}{}.\n", merged.title, refs::of(merged), name,
		                       id,
		                       not login ? ""
		                       : not read ? "; your entry there was not read (--offline)"
		                       : theirs   ? "; your entry there is folded in"
		                                  : "; it is new on your list there");
	// What your entry there brought is a step for undo; an id alone is not.
	journal::change folded{merged.type, merged.id, title, merged};
	int saved = journal::changedFields(folded).empty()
	                ? (ctx.db.saveAll() ? 0 : saveFailed(ctx))
	                : saveAndReport(ctx, line, {folded}, false);
	if (saved != 0 or not login)
		return saved;
	// The site gets all of it; the other sites what the fold changed.
	bool kept = ctx.db.queue({{.site = site, .type = merged.type, .id = merged.id}}) and
	            queueChanges(ctx, {folded});
	if (not kept)
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
	sendWaiting(ctx);
	return 0;
}

int runUnlink(const command& self, const args::parsed& line, cliContext& ctx) {
	auto request = readLink(self, line, ctx, 2);
	if (not request.title)
		return request.code;
	std::string name = providerName(request.site);
	auto title = heldAgain(ctx, *request.title);
	if (not title)
		return 1;
	if (idOn(*title, request.site) == 0) {
		ctx.err << std::format("{} is not linked to {}.\n", refs::of(*title), name);
		return 0;
	}
	ctx.db.updateEntry(withSiteId(*title, request.site, 0));
	if (not ctx.db.saveAll())
		return saveFailed(ctx);
	if (not line.has("q"))
		ctx.err << std::format("Unlinked {} ({}) from {}: it stays on your list there.\n",
		                       title->title, refs::of(*title), name);
	return 0;
}

}  // namespace

std::vector<command> syncCommands() {
	return {
	    {.name = "sync",
	     .summary = "Tell the sites what they have not heard of yet",
	     .details =
	         "Each change to a title (its status, score, progress, volumes, rewatches\n"
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
	     .run = runSync},
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
	     .run = runLink,
	     .completes = operandKind::kRefs},
	    {.name = "unlink",
	     .summary = "Stop telling a site of a title",
	     .details = "Forgets the title's id on the site, so changes to it are no longer\n"
	                "sent there. The site's entry stays as it is. Not a step for undo.\n",
	     .options = {{'a', "", "", "look in the anime list only"},
	                 {'m', "", "", "look in the manga list only"},
	                 kQuiet},
	     .operands = "REF anilist|mal|kitsu",
	     .run = runUnlink,
	     .completes = operandKind::kRefs},
	};
}

}  // namespace catalist::cli
