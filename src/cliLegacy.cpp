#include "cliLegacy.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <numeric>
#include <optional>
#include <ostream>

#include "malXml.hpp"
#include "refs.hpp"
#include "system.hpp"
#include "text.hpp"
#include "version.hpp"

namespace catalist {

namespace {

// ---- The shape of a command line ----

// An option a command takes: --name, -l for short, and what its value is
// called (empty for a flag that takes none).
struct option {
	std::string name;
	char letter = 0;
	std::string value;
	std::string help;
};

// The words and options of one command line, options by long name.
struct commandLine {
	std::vector<std::string> words;
	std::map<std::string, std::string> options;

	[[nodiscard]] bool has(const std::string& name) const { return options.count(name) > 0; }
	[[nodiscard]] const std::string& get(const std::string& name) const { return options.at(name); }
};

struct oldCommand {
	std::string name;
	std::vector<std::string> aliases;
	std::string usage;  // what follows "catalist "
	std::string summary;
	std::string details;
	std::vector<option> options;
	int (*run)(const commandLine& line, cliContext& ctx);
};

const option kHelpOption{"help", 'h', "", "show this help"};

// How many single-character edits turn one word into the other.
size_t editDistance(const std::string& from, const std::string& to) {
	std::vector<size_t> row(to.size() + 1);
	std::iota(row.begin(), row.end(), 0);
	for (size_t i = 1; i <= from.size(); ++i) {
		size_t diagonal = row[0];
		row[0] = i;
		for (size_t j = 1; j <= to.size(); ++j) {
			size_t above = row[j];
			size_t swap = diagonal + (from[i - 1] == to[j - 1] ? 0 : 1);
			row[j] = std::min({row[j] + 1, row[j - 1] + 1, swap});
			diagonal = above;
		}
	}
	return row[to.size()];
}

// The known word a typo was probably meant to be.
std::optional<std::string> closestTo(const std::string& typed,
                                     const std::vector<std::string>& known) {
	std::optional<std::string> best;
	size_t bestDistance = 3;  // further than two edits is not a typo
	for (const auto& word : known) {
		size_t distance = editDistance(typed, word);
		if (distance < bestDistance and distance < typed.size()) {
			best = word;
			bestDistance = distance;
		}
	}
	return best;
}

// Splits the arguments after the command name into words and options:
// --name value, --name=value, -l value, -lvalue, and -- to end the options.
// A word like -3 is a word, not an option.
result<commandLine> parseLine(const std::vector<std::string>& args, const oldCommand& cmd) {
	std::vector<option> options = cmd.options;
	options.push_back(kHelpOption);
	commandLine line;
	bool optionsOver = false;
	for (size_t i = 0; i < args.size(); ++i) {
		const std::string& arg = args[i];
		if (arg == "--" and not optionsOver) {
			optionsOver = true;
			continue;
		}
		bool isOption = not optionsOver and arg.size() > 1 and arg[0] == '-' and
		                not std::isdigit(static_cast<unsigned char>(arg[1]));
		if (not isOption) {
			line.words.push_back(arg);
			continue;
		}

		const option* found = nullptr;
		std::string value;
		bool valueGiven = false;
		if (arg.rfind("--", 0) == 0) {
			std::string name = arg.substr(2);
			auto equals = name.find('=');
			if (equals != std::string::npos) {
				value = name.substr(equals + 1);
				name = name.substr(0, equals);
				valueGiven = true;
			}
			for (const auto& candidate : options)
				if (candidate.name == name)
					found = &candidate;
			if (not found) {
				std::vector<std::string> names;
				for (const auto& candidate : options)
					names.push_back(candidate.name);
				auto guess = closestTo(name, names);
				return result<commandLine>::failure(
				    fmt::format("unknown option '--{}'{}", name,
				                guess ? fmt::format(" (did you mean '--{}'?)", *guess) : ""));
			}
		} else {
			for (const auto& candidate : options)
				if (candidate.letter == arg[1])
					found = &candidate;
			if (not found)
				return result<commandLine>::failure(fmt::format("unknown option '-{}'", arg[1]));
			if (arg.size() > 2) {
				value = arg.substr(2);
				valueGiven = true;
			}
		}

		if (found->value.empty()) {
			if (valueGiven)
				return result<commandLine>::failure(
				    fmt::format("--{} takes no value", found->name));
			line.options[found->name] = "";
			continue;
		}
		if (not valueGiven) {
			if (i + 1 >= args.size())
				return result<commandLine>::failure(
				    fmt::format("--{} needs a value ({})", found->name, found->value));
			value = args[++i];
		}
		line.options[found->name] = value;
	}
	return result<commandLine>::success(std::move(line));
}

// ---- Output ----

// Text in an SGR colour or style, or as it is when colours are off.
std::string paint(const cliContext& ctx, const std::string& text, const char* sgr) {
	return ctx.color ? fmt::format("\x1b[{}m{}\x1b[0m", sgr, text) : text;
}

const char* kBold = "1";
const char* kDim = "2";
const char* kGreen = "32";

std::string unitName(mediaType type, int count = 2) {
	if (type == mediaType::kAnime)
		return count == 1 ? "episode" : "episodes";
	return count == 1 ? "chapter" : "chapters";
}

// The title's id on the provider, or "-" when it has none there.
std::string idLabel(const entry& e, provider site) {
	int id = idOn(e, site);
	return id > 0 ? std::to_string(id) : "-";
}

// Tabs and line breaks would split a tab-separated line.
std::string oneField(std::string text) {
	std::replace_if(
	    text.begin(), text.end(), [](char c) { return c == '\t' or c == '\n' or c == '\r'; }, ' ');
	return text;
}

// A short line naming an entry, for lists of choices.
std::string entryLine(const entry& e, provider site) {
	return fmt::format("{}  {:>7}  {}", typeKey(e.type), idLabel(e, site), e.title);
}

int fail(cliContext& ctx, const std::string& message) {
	ctx.err << "error: " << message << "\n";
	return 1;
}

int usageError(cliContext& ctx, const oldCommand& cmd, const std::string& message) {
	ctx.err << "error: " << message << "\n"
	        << "Usage: catalist " << cmd.usage << "\n"
	        << "Run 'catalist help " << cmd.name << "' for more.\n";
	return 2;
}

// ---- Reading values ----

std::optional<mediaType> parseType(const std::string& word) {
	std::string lower = utf8::lowercase(word);
	if (lower == "anime")
		return mediaType::kAnime;
	if (lower == "manga")
		return mediaType::kManga;
	return std::nullopt;
}

// A count: digits only, and few enough of them to fit an int.
std::optional<int> parseCount(const std::string& text) {
	if (text.empty() or text.size() > 6 or
	    not std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); }))
		return std::nullopt;
	return std::stoi(text);
}

// Takes "anime" or "manga" off the front of the words, when it is there.
std::optional<mediaType> takeType(std::vector<std::string>& words) {
	if (words.empty())
		return std::nullopt;
	auto type = parseType(words.front());
	if (type)
		words.erase(words.begin());
	return type;
}

std::string joined(const std::vector<std::string>& words) {
	std::string text;
	for (const auto& word : words)
		text += text.empty() ? word : fmt::format(" {}", word);
	return text;
}

std::vector<mediaType> typesFor(std::optional<mediaType> type) {
	if (type)
		return {*type};
	return {mediaType::kAnime, mediaType::kManga};
}

bool titleHasWords(const entry& e, const std::vector<std::string>& words) {
	std::string title = utf8::lowercase(e.title);
	return std::all_of(words.begin(), words.end(), [&title](const std::string& word) {
		return title.find(utf8::lowercase(word)) != std::string::npos;
	});
}

// ---- Finding an entry ----

// The entries the words name: the one with that id on the provider when the
// words are one number, else those whose title has every word (a title that
// is exactly the words wins on its own).
std::vector<entry> entriesNamed(const catalogue& db, std::optional<mediaType> type,
                                const std::vector<std::string>& words) {
	std::vector<entry> found;
	if (words.size() == 1) {
		if (auto id = parseCount(words[0])) {
			for (mediaType each : typesFor(type))
				for (const auto& e : db.getEntries(each))
					if (*id > 0 and idOn(e, db.getProvider()) == *id)
						found.push_back(e);
			if (not found.empty())
				return found;
		}
	}
	std::string whole = utf8::lowercase(joined(words));
	std::vector<entry> exact;
	for (mediaType each : typesFor(type)) {
		for (const auto& e : db.getEntries(each)) {
			if (utf8::lowercase(e.title) == whole)
				exact.push_back(e);
			else if (titleHasWords(e, words))
				found.push_back(e);
		}
	}
	return exact.empty() ? found : exact;
}

// Asks which of the numbered choices was meant; nothing when the answer is
// empty or not one of them.
std::optional<size_t> askWhich(cliContext& ctx, size_t count) {
	ctx.err << fmt::format("Which one? [1-{}, Enter to cancel]: ", count) << std::flush;
	std::string answer;
	if (not std::getline(ctx.in, answer))
		return std::nullopt;
	auto picked = parseCount(answer);
	if (not picked or *picked < 1 or static_cast<size_t>(*picked) > count)
		return std::nullopt;
	return static_cast<size_t>(*picked - 1);
}

// The one entry the words name. Several are offered to choose from at a
// terminal; otherwise, and when none matches, the reason is printed.
std::optional<entry> findOne(cliContext& ctx, std::optional<mediaType> type,
                             const std::vector<std::string>& words) {
	auto found = entriesNamed(ctx.db, type, words);
	std::string asked = joined(words);
	if (found.empty()) {
		fail(ctx, fmt::format("nothing in your lists matches '{}'", asked));
		ctx.err << "See what is there with: catalist ls\n";
		return std::nullopt;
	}
	if (found.size() == 1)
		return found.front();

	provider site = ctx.db.getProvider();
	if (not ctx.interactive) {
		fail(ctx, fmt::format("'{}' matches {} titles:", asked, found.size()));
		for (const auto& e : found)
			ctx.err << "  " << entryLine(e, site) << "\n";
		ctx.err << fmt::format("Name one by its {} id, or with more of its title.\n",
		                       providerName(site));
		return std::nullopt;
	}
	ctx.err << fmt::format("'{}' matches {} titles:\n", asked, found.size());
	for (size_t i = 0; i < found.size(); ++i)
		ctx.err << fmt::format("  {:>2}  {}\n", i + 1, entryLine(found[i], site));
	auto picked = askWhich(ctx, found.size());
	if (not picked) {
		ctx.err << "Canceled.\n";
		return std::nullopt;
	}
	return found[*picked];
}

bool load(cliContext& ctx) {
	auto loaded = ctx.db.loadAll();
	if (not loaded.ok())
		fail(ctx, loaded.errorMessage());
	ctx.err << importNote(ctx.db);
	return loaded.ok();
}

bool save(cliContext& ctx) {
	if (ctx.db.saveAll(undoNote(ctx)))
		return true;
	fail(ctx, fmt::format("could not save the catalogue to {}", ctx.db.getDataDir().string()));
	return false;
}

// ---- Changing an entry ----

const std::vector<option> kEditOptions = {
    {"status", 's', "STATUS", "watching, completed, on-hold, dropped or planned"},
    {"rating", 'r', "N", "your rating, 1 to 10 (0 clears it)"},
    {"progress", 'p', "N", "episodes or chapters done; +N or -N moves it"},
    {"total", 't', "N", "episodes or chapters in all"},
    {"volumes", 0, "N", "volumes read (manga)"},
    {"rewatched", 0, "N", "times rewatched or reread"},
};

// Applies the edit options in a fixed order (the total first, so progress
// can reach it; the status before progress, so a given progress stands).
result<entry> applyEdits(entry target, const commandLine& line, const isoDate& today) {
	auto count = [&line](const std::string& name) { return parseCount(line.get(name)); };
	auto bad = [&line](const std::string& name, const std::string& wanted) {
		return result<entry>::failure(
		    fmt::format("--{} '{}' is not {}", name, line.get(name), wanted));
	};

	if (line.has("total")) {
		if (not count("total"))
			return bad("total", "a count");
		target = setCount(target, countField::kTotalUnits, *count("total"), today);
	}
	if (line.has("status")) {
		auto status = parseStatus(line.get("status"));
		if (not status)
			return bad("status", "watching, completed, on-hold, dropped or planned");
		target = setStatus(target, *status, today);
	}
	if (line.has("progress")) {
		const std::string& value = line.get("progress");
		bool relative = not value.empty() and (value[0] == '+' or value[0] == '-');
		auto amount = parseCount(relative ? value.substr(1) : value);
		if (not amount)
			return bad("progress", "a count, +N or -N");
		if (relative)
			target = stepProgress(target, value[0] == '+' ? *amount : -*amount, today);
		else
			target = setCount(target, countField::kProgress, *amount, today);
	}
	if (line.has("rating")) {
		if (not count("rating") or *count("rating") > 10)
			return bad("rating", "0 to 10");
		target = setScore(target, *count("rating"), today);
	}
	if (line.has("volumes")) {
		if (not count("volumes"))
			return bad("volumes", "a count");
		target = setCount(target, countField::kVolumes, *count("volumes"), today);
	}
	if (line.has("rewatched")) {
		if (not count("rewatched"))
			return bad("rewatched", "a count");
		target = setCount(target, countField::kRewatched, *count("rewatched"), today);
	}
	return result<entry>::success(std::move(target));
}

// ---- Commands ----

int runAdd(const commandLine& line, cliContext& ctx);
int runRemove(const commandLine& line, cliContext& ctx);
int runSearch(const commandLine& line, cliContext& ctx);
int runProvider(const commandLine& line, cliContext& ctx);
int runImport(const commandLine& line, cliContext& ctx);
int runExport(const commandLine& line, cliContext& ctx);

std::vector<option> withEdits(std::vector<option> options) {
	options.insert(options.end(), kEditOptions.begin(), kEditOptions.end());
	return options;
}

const std::vector<oldCommand>& oldCommands() {
	static const std::vector<oldCommand> all = {
	    {"add",
	     {},
	     "add <anime|manga> <TITLE|ID> [options]",
	     "Add a title found on AniList",
	     "Searches AniList for the title and adds the match you pick (the only\n"
	     "match, or the first with --yes). A number is taken as the title's id on\n"
	     "your provider. New titles are planned unless --status says otherwise.\n",
	     withEdits({{"yes", 'y', "", "take the best match without asking"}}),
	     runAdd},
	    {"rm",
	     {"remove", "delete"},
	     "rm [anime|manga] <TITLE|ID> [options]",
	     "Remove a title",
	     "",
	     {{"yes", 'y', "", "remove without asking"}},
	     runRemove},
	    {"search",
	     {},
	     "search <anime|manga> <WORDS...>",
	     "Search AniList without adding anything",
	     "",
	     {},
	     runSearch},
	    {"provider",
	     {},
	     "provider [anilist|mal]",
	     "Show or choose your provider",
	     "Your provider is the site whose ids Catalist shows and accepts: AniList\n"
	     "(the default) or MyAnimeList. Syncing with your account there is not\n"
	     "done yet; your lists stay on this computer.\n",
	     {},
	     runProvider},
	    {"import",
	     {},
	     "import <FILE.xml>...",
	     "Add the titles of MyAnimeList XML exports",
	     "A title already in your lists keeps your rating, progress and dates;\n"
	     "only its totals are refreshed. MyAnimeList gzips its exports: unpack\n"
	     "them first (gunzip).\n",
	     {},
	     runImport},
	    {"export",
	     {},
	     "export <anime|manga>",
	     "Print a list as MyAnimeList XML",
	     "Both MyAnimeList and AniList read the file on their import pages. Titles\n"
	     "without a MyAnimeList id are left out, with a note.\n",
	     {},
	     runExport},
	};
	return all;
}

const oldCommand* findCommand(const std::string& name) {
	for (const auto& each : oldCommands()) {
		if (each.name == name or
		    std::find(each.aliases.begin(), each.aliases.end(), name) != each.aliases.end())
			return &each;
	}
	return nullptr;
}

// One of the commands above, by its name.
const oldCommand& commandNamed(const char* name) {
	return *findCommand(name);
}

void printCommandHelp(std::ostream& out, const oldCommand& cmd) {
	out << cmd.summary << ".\n\nUsage: catalist " << cmd.usage << "\n";
	if (not cmd.aliases.empty()) {
		out << "Also: ";
		for (size_t i = 0; i < cmd.aliases.size(); ++i)
			out << (i > 0 ? ", " : "") << cmd.aliases[i];
		out << "\n";
	}
	if (not cmd.details.empty())
		out << "\n" << cmd.details;
	std::vector<option> options = cmd.options;
	options.push_back(kHelpOption);
	out << "\nOptions:\n";
	for (const auto& each : options) {
		std::string flags = each.letter ? fmt::format("-{}, --{}", each.letter, each.name)
		                                : fmt::format("    --{}", each.name);
		if (not each.value.empty())
			flags += fmt::format(" {}", each.value);
		out << fmt::format("  {:<24}{}\n", flags, each.help);
	}
}

// The id a search match has on the provider, or "-".
std::string matchId(const anilist::title& match, provider site) {
	int id = site == provider::kAniList ? match.anilistId : match.malId;
	return id > 0 ? std::to_string(id) : "-";
}

std::string matchLine(const anilist::title& match, provider site, mediaType type) {
	std::string length = match.totalUnits > 0 ? fmt::format("{} {}", match.totalUnits,
	                                                        unitName(type, match.totalUnits))
	                                          : "running";
	return fmt::format("{:>7}  {:<4}  {}  ({})", matchId(match, site),
	                   match.year.empty() ? "-" : match.year, match.name, length);
}

int runAdd(const commandLine& line, cliContext& ctx) {
	const oldCommand& cmd = commandNamed("add");
	std::vector<std::string> words = line.words;
	auto type = takeType(words);
	if (not type)
		return usageError(ctx, cmd,
		                  "say which list: catalist add anime ... or catalist add manga ...");
	if (words.empty())
		return usageError(ctx, cmd, "name the title to add");
	entry blank;
	blank.type = *type;
	auto checked = applyEdits(blank, line, ctx.today());
	if (not checked.ok())
		return usageError(ctx, cmd, checked.errorMessage());
	if (not load(ctx))
		return 1;

	provider site = ctx.db.getProvider();
	std::string asked = joined(words);
	const std::string kOffline = "could not reach AniList; check the connection and try again";

	// A number is the title's id on the provider; anything else is searched.
	anilist::titleRef wanted{*type, 0, 0};
	if (auto id = words.size() == 1 ? parseCount(words[0]) : std::optional<int>()) {
		(site == provider::kAniList ? wanted.anilistId : wanted.malId) = *id;
	} else {
		auto found = anilist::search(ctx.post, *type, asked);
		if (not found.ok())
			return fail(ctx, kOffline);
		const auto& matches = found.value();
		if (matches.empty())
			return fail(ctx, fmt::format("AniList has no {} matching '{}'", typeKey(*type), asked));
		size_t pick = 0;
		if (matches.size() > 1 and not line.has("yes")) {
			if (not ctx.interactive) {
				fail(ctx, fmt::format("'{}' matches {} titles on AniList:", asked, matches.size()));
				for (const auto& match : matches)
					ctx.err << "  " << matchLine(match, site, *type) << "\n";
				ctx.err << fmt::format(
				    "Add one by its {} id, e.g. catalist add {} {}, or pass "
				    "--yes for the first.\n",
				    providerName(site), typeKey(*type), matchId(matches.front(), site));
				return 1;
			}
			for (size_t i = 0; i < matches.size(); ++i) {
				bool tracked =
				    ctx.db.findByIds(*type, matches[i].anilistId, matches[i].malId).has_value();
				ctx.err << fmt::format("  {:>2}  {}{}\n", i + 1, matchLine(matches[i], site, *type),
				                       tracked ? paint(ctx, "  in your list", kGreen) : "");
			}
			auto chosen = askWhich(ctx, matches.size());
			if (not chosen) {
				ctx.err << "Canceled.\n";
				return 1;
			}
			pick = *chosen;
		}
		wanted.anilistId = matches[pick].anilistId;
		wanted.malId = matches[pick].malId;
	}

	// Looked up in full, for the name, both ids and how much is out.
	auto details = anilist::lookUp(ctx.post, wanted);
	if (not details.ok()) {
		if (details.errorMessage() == "could not reach AniList")
			return fail(ctx, kOffline);
		return fail(ctx, fmt::format("AniList knows no {} with {} id {}", typeKey(*type),
		                             providerName(site), asked));
	}
	const auto& facts = details.value().facts;
	if (auto existing = ctx.db.findByIds(*type, facts.anilistId, facts.malId)) {
		fail(ctx, fmt::format("{} is already in your {} list ({})", existing->title, typeKey(*type),
		                      statusToString(existing->status, existing->type)));
		ctx.err << fmt::format("Change it with: {} set {} ...\n", ctx.program, refs::of(*existing));
		return 1;
	}

	entry added;
	added.type = *type;
	added.title = details.value().name.empty() ? asked : details.value().name;
	added.status = watchStatus::kPlanToWatch;
	added.updatedAt = ctx.today();
	added = applyWebFacts(added, facts, ctx.today());
	added = applyEdits(added, line, ctx.today()).value();
	ctx.db.addEntry(added);
	if (not save(ctx))
		return 1;
	ctx.out << fmt::format("Added {} to your {} list as {}.\n", paint(ctx, added.title, kBold),
	                       typeKey(*type), statusToString(added.status, added.type));
	return 0;
}

int runRemove(const commandLine& line, cliContext& ctx) {
	std::vector<std::string> words = line.words;
	auto type = takeType(words);
	if (words.empty())
		return usageError(ctx, commandNamed("rm"), "name a title");
	if (not load(ctx))
		return 1;
	auto found = findOne(ctx, type, words);
	if (not found)
		return 1;

	if (not line.has("yes")) {
		if (not ctx.interactive)
			return fail(ctx,
			            fmt::format("pass --yes to remove {} without being asked", found->title));
		ctx.err << fmt::format("Remove {} from your {} list? [y/N]: ", found->title,
		                       typeKey(found->type))
		        << std::flush;
		std::string answer;
		std::getline(ctx.in, answer);
		if (utf8::lowercase(answer) != "y" and utf8::lowercase(answer) != "yes") {
			ctx.err << "Kept.\n";
			return 1;
		}
	}
	ctx.db.removeEntry(found->type, found->id);
	if (not save(ctx))
		return 1;
	ctx.out << fmt::format("Removed {}.\n", found->title);
	return 0;
}

int runSearch(const commandLine& line, cliContext& ctx) {
	const oldCommand& cmd = commandNamed("search");
	std::vector<std::string> words = line.words;
	auto type = takeType(words);
	if (not type)
		return usageError(ctx, cmd, "say which kind: catalist search anime ... or manga ...");
	if (words.empty())
		return usageError(ctx, cmd, "give words to search for");
	if (not load(ctx))
		return 1;
	auto found = anilist::search(ctx.post, *type, joined(words));
	if (not found.ok())
		return fail(ctx, "could not reach AniList; check the connection and try again");

	provider site = ctx.db.getProvider();
	for (const auto& match : found.value()) {
		bool tracked = ctx.db.findByIds(*type, match.anilistId, match.malId).has_value();
		if (ctx.columns <= 0) {
			ctx.out << fmt::format("{}\t{}\t{}\t{}\t{}\n", matchId(match, site), match.year,
			                       oneField(match.name), match.totalUnits,
			                       tracked ? "tracked" : "");
			continue;
		}
		ctx.out << matchLine(match, site, *type)
		        << (tracked ? paint(ctx, "  in your list", kGreen) : "") << "\n";
	}
	if (ctx.columns > 0 and found.value().empty())
		ctx.out << paint(ctx, "No matches.", kDim) << "\n";
	else if (ctx.columns > 0)
		ctx.out << paint(ctx,
		                 fmt::format("Add one with: catalist add {} <{} id>", typeKey(*type),
		                             providerName(site)),
		                 kDim)
		        << "\n";
	return 0;
}

int runProvider(const commandLine& line, cliContext& ctx) {
	const oldCommand& cmd = commandNamed("provider");
	if (line.words.size() > 1)
		return usageError(ctx, cmd, "name one provider: anilist or mal");
	std::optional<provider> chosen;
	if (not line.words.empty()) {
		chosen = parseProvider(line.words[0]);
		if (not chosen)
			return usageError(
			    ctx, cmd,
			    fmt::format("unknown provider '{}'; choose anilist or mal", line.words[0]));
	}
	if (not load(ctx))
		return 1;
	if (not chosen) {
		ctx.out << fmt::format("Your provider is {}.\n",
		                       paint(ctx, providerName(ctx.db.getProvider()), kBold));
		return 0;
	}
	ctx.db.setProvider(*chosen);
	if (not ctx.db.saveSettings())
		return fail(ctx,
		            fmt::format("could not save the settings in {}", ctx.db.getDataDir().string()));
	ctx.out << fmt::format("Your provider is now {}: its ids are the ones shown and accepted.\n",
	                       paint(ctx, providerName(*chosen), kBold));
	return 0;
}

int runImport(const commandLine& line, cliContext& ctx) {
	if (line.words.empty())
		return usageError(ctx, commandNamed("import"), "give at least one file");
	if (not load(ctx))
		return 1;
	// Everything is read before anything is added, so a bad file leaves the
	// catalogue untouched.
	std::vector<std::vector<entry>> lists;
	for (const auto& file : line.words) {
		auto content = readFile(file);
		if (not content)
			return fail(ctx, fmt::format("cannot read {}", file));
		// MyAnimeList hands out its exports gzipped.
		if (content->rfind("\x1f\x8b", 0) == 0)
			return fail(ctx,
			            fmt::format("{} is compressed; unpack it first (gunzip {})", file, file));
		lists.push_back(malXml::parse(*content));
		if (lists.back().empty())
			return fail(ctx, fmt::format("no anime or manga found in {} (is it a MyAnimeList XML "
			                             "export?)",
			                             file));
	}

	for (size_t i = 0; i < line.words.size(); ++i) {
		size_t added = 0;
		for (const auto& title : lists[i]) {
			size_t before = ctx.db.getEntries(title.type).size();
			ctx.db.addEntry(title);
			added += ctx.db.getEntries(title.type).size() - before;
		}
		ctx.out << fmt::format("{}: {} titles, {} added, {} already in the catalogue\n",
		                       line.words[i], lists[i].size(), added, lists[i].size() - added);
	}
	if (not save(ctx))
		return 1;
	// The new titles have only MyAnimeList ids: look the others up on the
	// next command instead of tomorrow.
	ctx.db.setLinkedOn(std::nullopt);
	(void)ctx.db.saveSettings();
	return 0;
}

int runExport(const commandLine& line, cliContext& ctx) {
	auto type = line.words.size() == 1 ? parseType(line.words[0]) : std::nullopt;
	if (not type)
		return usageError(ctx, commandNamed("export"), "say which list: anime or manga");
	if (not load(ctx))
		return 1;
	auto list = malXml::write(ctx.db.getEntries(*type), *type);
	ctx.out << list.xml;
	if (list.skipped > 0)
		ctx.err << "note: " << list.skipped << " titles without a MyAnimeList id were left out.\n";
	return 0;
}

}  // namespace

std::vector<std::pair<std::string, std::string>> legacySummaries() {
	std::vector<std::pair<std::string, std::string>> all;
	for (const auto& each : oldCommands())
		all.emplace_back(each.name, each.summary);
	return all;
}

bool printLegacyHelp(const std::string& name, std::ostream& out) {
	const oldCommand* asked = findCommand(name);
	if (asked)
		printCommandHelp(out, *asked);
	return asked != nullptr;
}

std::optional<int> runLegacyCli(const std::vector<std::string>& args, cliContext& ctx) {
	const oldCommand* cmd = findCommand(args.at(0));
	if (not cmd)
		return std::nullopt;
	auto line = parseLine({args.begin() + 1, args.end()}, *cmd);
	if (not line.ok())
		return usageError(ctx, *cmd, line.errorMessage());
	if (line.value().has("help")) {
		printCommandHelp(ctx.out, *cmd);
		return 0;
	}
	return cmd->run(line.value(), ctx);
}

}  // namespace catalist
