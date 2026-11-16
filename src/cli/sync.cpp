#include <fmt/format.h>

#include <algorithm>
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

// Tells one site what waits for it; the count told, and adds what still
// waits to `left`.
int sendTo(cliContext& ctx, provider site, const std::vector<pending>& mine,
           std::vector<account>& logins, bool& loginsChanged, size_t& left) {
	auto login = accounts::on(logins, site);
	// Logged out since: nobody to tell.
	if (not login) {
		for (const auto& each : mine)
			(void)ctx.db.sent(each);
		return 0;
	}
	auto tracker = trackerFor(ctx, site);
	auto fresh = tracker->current(*login);
	if (not fresh.ok()) {
		ctx.err << "note: " << fresh.errorMessage() << ".\n";
		left += mine.size();
		return 0;
	}
	if (fresh.value().token != login->token) {
		logins = accounts::with(logins, fresh.value());
		loginsChanged = true;
	}
	int told = 0;
	for (size_t i = 0; i < mine.size(); ++i) {
		const pending& each = mine[i];
		if (i > 0)
			ctx.net.pause();
		result<bool> done = result<bool>::success(true);
		if (each.removed) {
			done = tracker->remove(each.type, each.siteId, fresh.value());
		} else if (auto now = ctx.db.getById(each.type, each.id)) {
			done = tracker->push(*now, fresh.value());
		} else {
			// Removed from the lists since, and not from the site.
			(void)ctx.db.sent(each);
			continue;
		}
		if (done.ok() or done.errorMessage() == web::notLinked(site)) {
			(void)ctx.db.sent(each);
			told += done.ok() ? 1 : 0;
			continue;
		}
		// The site is not there, or said no: this one waits, and so does the
		// rest for this site.
		ctx.err << fmt::format("note: could not tell {} of {}: {}.\n", providerName(site),
		                       refOf(each), done.errorMessage());
		left += mine.size() - i;
		break;
	}
	return told;
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
	bool loginsChanged = false;
	for (provider site : kSites) {
		std::vector<pending> mine;
		std::copy_if(waiting.begin(), waiting.end(), std::back_inserter(mine),
		             [site](const pending& each) { return each.site == site; });
		if (not mine.empty())
			report.told += sendTo(ctx, site, mine, logins, loginsChanged, report.left);
	}
	// A renewed token is kept for next time.
	if (loginsChanged and not saveAccounts(ctx, logins))
		ctx.err << "note: could not save the renewed login.\n";
	return report;
}

void noteWaiting(cliContext& ctx, const sendReport& report) {
	if (report.left == 0 or ctx.offline)
		return;
	ctx.err << fmt::format(
	    "note: {} to be sent to the sites; '{} sync' sends {}.\n",
	    report.left == 1 ? "1 change waits" : fmt::format("{} changes wait", report.left),
	    ctx.program, report.left == 1 ? "it" : "them");
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
		ctx.err << fmt::format(
		    "Forgot {} the sites were not told of.\n",
		    waiting.size() == 1 ? "1 change" : fmt::format("{} changes", waiting.size()));
		return 0;
	}
	auto logins = readAccounts(ctx);
	if (logins.empty()) {
		ctx.err << fmt::format("You are not logged in to any site. Log in with: {} login SITE\n",
		                       ctx.program);
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
			ctx.err << fmt::format(
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
			ctx.err << fmt::format(
			    "Told the sites of {}.\n",
			    report.told == 1 ? "1 change" : fmt::format("{} changes", report.told));
	}
	if (report.left > 0)
		return fail(ctx, fmt::format("{} still to be sent; try again later",
		                             report.left == 1 ? "1 change"
		                                              : fmt::format("{} changes", report.left)));
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
		request.code = usageError(ctx, self, fmt::format("give {}", self.operands));
		return request;
	}
	auto site = parseProvider(line.operands[1]);
	if (not site) {
		request.code = usageError(
		    ctx, self,
		    fmt::format("unknown site '{}'; choose anilist, mal or kitsu", line.operands[1]));
		return request;
	}
	request.site = *site;
	if (not loadForWrite(ctx)) {
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
	entry title = *request.title;
	std::string name = providerName(request.site);
	if (line.operands.size() == 3) {
		auto id = args::parseCount(line.operands[2]);
		if (not id or *id == 0)
			return usageError(ctx, self,
			                  fmt::format("'{}' is not an id on {}", line.operands[2], name));
		title = withSiteId(title, request.site, *id);
	} else {
		// Searched by its title, the person choosing as for add.
		auto site = trackerFor(ctx, request.site);
		auto match = chooseMatch(ctx, *site, title.type, title.title, std::nullopt);
		if (not match)
			return 1;
		// The match's id on the site, and the other ids it knows that the
		// title lacks.
		entry chosen;
		chosen.anilistId = match->anilistId;
		chosen.malId = match->malId;
		chosen.kitsuId = match->kitsuId;
		for (provider other : kSites)
			if (idOn(title, other) == 0)
				title = withSiteId(title, other, idOn(chosen, other));
		title = withSiteId(title, request.site, idOn(chosen, request.site));
	}
	int id = idOn(title, request.site);
	for (const auto& other : ctx.db.getEntries(title.type))
		if (other.id != title.id and idOn(other, request.site) == id)
			return fail(ctx, fmt::format("{} is {} {} already", refs::of(other), name, id));
	ctx.db.updateEntry(title);
	// An id is no step for undo: undo puts back what you did to a title.
	if (not ctx.db.saveAll())
		return saveFailed(ctx);
	if (not line.has("q"))
		ctx.err << fmt::format("Linked {} ({}) to {} {}.\n", title.title, refs::of(title), name,
		                       id);
	tellSites(ctx, {{title.type, title.id, std::nullopt, title}});
	return 0;
}

int runUnlink(const command& self, const args::parsed& line, cliContext& ctx) {
	auto request = readLink(self, line, ctx, 2);
	if (not request.title)
		return request.code;
	std::string name = providerName(request.site);
	if (idOn(*request.title, request.site) == 0) {
		ctx.err << fmt::format("{} is not linked to {}.\n", refs::of(*request.title), name);
		return 0;
	}
	ctx.db.updateEntry(withSiteId(*request.title, request.site, 0));
	if (not ctx.db.saveAll())
		return saveFailed(ctx);
	if (not line.has("q"))
		ctx.err << fmt::format("Unlinked {} ({}) from {}: it stays on your list there.\n",
		                       request.title->title, refs::of(*request.title), name);
	return 0;
}

}  // namespace

std::vector<command> syncCommands() {
	return {
	    {"sync",
	     {},
	     "Tell the sites what they have not heard of yet",
	     "Each change to a title (its status, score, progress, volumes,\n"
	     "rewatches or dates) is sent at once to every site you are logged in to\n"
	     "where the title is linked, as Mihon tells its trackers: your lists are\n"
	     "the truth, and the site's entry is overwritten. What could not be sent\n"
	     "(offline, a site down, a login run out, or more than ten changes at\n"
	     "once) waits, and sync sends it. With --all every linked title is sent,\n"
	     "as after a first login.\n",
	     {{0, "all", "", "send every linked title, not only what waits"},
	      {0, "forget", "", "drop what waits, telling the sites nothing"},
	      kQuiet},
	     "",
	     runSync},
	    {"link",
	     {},
	     "Link a title to its entry on a site",
	     "Sets the title's id on the site: the ID given, or the match you choose\n"
	     "of a search for its title there. A linked title is sent to that site\n"
	     "when it changes (see 'sync'). Titles added from a site, imported or\n"
	     "refreshed are linked to every site that knew their ids. Not a step\n"
	     "for undo.\n",
	     {{'a', "", "", "look in the anime list only"},
	      {'m', "", "", "look in the manga list only"},
	      kQuiet},
	     "REF anilist|mal|kitsu [ID]",
	     runLink,
	     operandKind::kRefs},
	    {"unlink",
	     {},
	     "Stop telling a site of a title",
	     "Forgets the title's id on the site, so changes to it are no longer\n"
	     "sent there. The site's entry stays as it is. Not a step for undo.\n",
	     {{'a', "", "", "look in the anime list only"},
	      {'m', "", "", "look in the manga list only"},
	      kQuiet},
	     "REF anilist|mal|kitsu",
	     runUnlink,
	     operandKind::kRefs},
	};
}

}  // namespace catalist::cli
