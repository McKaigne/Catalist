#include <algorithm>
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

const std::vector<provider> kSites = {provider::kAniList, provider::kMyAnimeList, provider::kKitsu};

// The fields the sites keep of a title: a change to any other (a title, a
// total) is not theirs to hear of.
bool concernsSites(const journal::change& made) {
	static const std::vector<std::string> kKept = {"added",   "status",    "score",   "progress",
	                                               "volumes", "rewatched", "started", "finished"};
	auto fields = journal::changedFields(made);
	return std::any_of(fields.begin(), fields.end(), [](const journal::fieldChange& field) {
		return std::find(kKept.begin(), kKept.end(), field.field) != kKept.end();
	});
}

std::string refOf(const pending& waiting) {
	return refs::of(waiting.type, waiting.id);
}

// What telling one site did: how many titles it was told of, how many still
// wait, and the login when its token was renewed on the way.
struct siteReport {
	size_t told = 0;
	size_t left = 0;
	std::optional<account> renewed;
};

// Tells one site what waits for it, in order, until one cannot be told.
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
			sent = tracker->push(*now, *fresh);
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
		ctx.err << std::format("note: could not tell {} of {}: {}.\n", providerName(site),
		                       refOf(each), sent.error().message);
		// The site said no to this title only: it waits, and the rest go on.
		if (sent.error().why == web::failure::kind::kRefused) {
			++report.left;
			continue;
		}
		// The site is not there, or no longer takes the login: the rest wait
		// too.
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
	if (ctx.offline or (not everything and waiting.size() > kSendAtOnce)) {
		report.left = waiting.size();
		return report;
	}
	auto logins = readAccounts(ctx);
	bool renewed = false;
	for (provider site : kSites) {
		std::vector<pending> mine;
		std::copy_if(waiting.begin(), waiting.end(), std::back_inserter(mine),
		             [site](const pending& each) { return each.site == site; });
		if (mine.empty())
			continue;
		auto sent = sendTo(ctx, site, mine, accounts::on(logins, site));
		report.told += sent.told;
		report.left += sent.left;
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

void tellSites(cliContext& ctx, const std::vector<journal::change>& changes) {
	auto logins = readAccounts(ctx);
	if (logins.empty())
		return;
	std::vector<pending> more;
	for (const auto& made : changes) {
		if (not made.after or not concernsSites(made))
			continue;
		for (const auto& login : logins)
			if (idOn(*made.after, login.site) > 0)
				more.push_back({login.site, made.type, made.id});
	}
	if (more.empty())
		return;
	if (not ctx.db.queue(more)) {
		ctx.err << "note: could not keep the changes to tell the sites of.\n";
		return;
	}
	noteWaiting(ctx, sendOutbox(ctx, false));
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

int runSync(const command& self, const args::parsed& line, cliContext& ctx) {
	if (not line.operands.empty())
		return usageError(ctx, self, "sync takes no operands");
	if (line.has("all") and line.has("forget"))
		return usageError(ctx, self, "give --all or --forget, not both");
	if (not load(ctx))
		return 1;
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
		if (report.told == 0 and report.left == 0)
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
	return 0;
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

int runLink(const command& self, const args::parsed& line, cliContext& ctx) {
	auto request = readLink(self, line, ctx, 3);
	if (not request.title)
		return request.code;
	std::string name = providerName(request.site);
	// The ids to give the title: the one given, or those of the match
	// chosen of a search for its title, the person choosing as for add.
	entry chosen;
	if (line.operands.size() == 3) {
		auto id = args::parseCount(line.operands[2]);
		if (not id or *id == 0)
			return usageError(ctx, self,
			                  std::format("'{}' is not an id on {}", line.operands[2], name));
		chosen = withSiteId(chosen, request.site, *id);
	} else {
		auto site = trackerFor(ctx, request.site);
		auto match = chooseMatch(ctx, *site, request.title->type, request.title->title,
		                         std::nullopt);
		if (not match)
			return 1;
		chosen.anilistId = match->anilistId;
		chosen.malId = match->malId;
		chosen.kitsuId = match->kitsuId;
	}
	auto held = heldAgain(ctx, *request.title);
	if (not held)
		return 1;
	// The id on the site, and the other ids the match knows that the title
	// lacks.
	entry title = *held;
	for (provider other : kSites)
		if (idOn(title, other) == 0)
			title = withSiteId(title, other, idOn(chosen, other));
	title = withSiteId(title, request.site, idOn(chosen, request.site));
	int id = idOn(title, request.site);
	for (const auto& other : ctx.db.getEntries(title.type))
		if (other.id != title.id and idOn(other, request.site) == id)
			return fail(ctx, std::format("{} is {} {} already", refs::of(other), name, id));
	ctx.db.updateEntry(title);
	// An id is no step for undo: undo puts back what you did to a title.
	if (not ctx.db.saveAll())
		return saveFailed(ctx);
	if (not line.has("q"))
		ctx.err << std::format("Linked {} ({}) to {} {}.\n", title.title, refs::of(title), name,
		                       id);
	tellSites(ctx, {{title.type, title.id, std::nullopt, title}});
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
	     .details = "Each change to a title (its status, score, progress, volumes,\n"
	                "rewatches or dates) is sent at once to every site you are logged in to\n"
	                "where the title is linked, as Mihon tells its trackers: your lists are\n"
	                "the truth, and the site's entry is overwritten. What could not be sent\n"
	                "(offline, a site down, a login run out, or more than ten changes at\n"
	                "once) waits, and sync sends it. With --all every linked title is sent,\n"
	                "as after a first login.\n",
	     .options = {{0, "all", "", "send every linked title, not only what waits"},
	                 {0, "forget", "", "drop what waits, telling the sites nothing"},
	                 kQuiet},
	     .run = runSync},
	    {.name = "link",
	     .summary = "Link a title to its entry on a site",
	     .details = "Sets the title's id on the site: the ID given, or the match you choose\n"
	                "of a search for its title there. A linked title is sent to that site\n"
	                "when it changes (see 'sync'). Titles added from a site, imported or\n"
	                "refreshed are linked to every site that knew their ids. Not a step\n"
	                "for undo.\n",
	     .options = {{'a', "", "", "look in the anime list only"},
	                 {'m', "", "", "look in the manga list only"},
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
