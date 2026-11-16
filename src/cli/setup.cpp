#include <format>
#include <istream>
#include <iterator>
#include <map>
#include <ostream>

#include "cli/common.hpp"
#include "cli/completion.hpp"
#include "cli/refs.hpp"
#include "core/version.hpp"

namespace catalist::cli {

namespace {

int runProvider(const command& self, const args::parsed& line, cliContext& ctx) {
	if (line.operands.size() > 1)
		return usageError(ctx, self, "name one provider: anilist, mal or kitsu");
	if (line.operands.empty()) {
		if (not load(ctx))
			return 1;
		ctx.out << providerKey(ctx.db.getProvider()) << "\n";
		return 0;
	}
	auto chosen = parseProvider(line.operands[0]);
	if (not chosen)
		return usageError(ctx, self,
		                  std::format("unknown provider '{}'{}; choose anilist, mal or kitsu",
		                              line.operands[0], didYouMean(line.operands[0], self.words)));
	if (not loadForWrite(ctx))
		return 1;
	if (ctx.db.getProvider() == *chosen) {
		ctx.err << "Nothing changed.\n";
		return 0;
	}
	ctx.db.setProvider(*chosen);
	if (not ctx.db.saveSettings())
		return saveFailed(ctx);
	ctx.err << std::format("Your provider is now {}.\n", providerName(*chosen));
	return 0;
}

int runCompletion(const command& self, const args::parsed& line, cliContext& ctx) {
	std::string shell = line.operands.size() == 1 ? line.operands[0] : "";
	if (shell == "zsh")
		ctx.out << completion::zsh(commands());
	else if (shell == "fish")
		ctx.out << completion::fish(commands());
	else
		return usageError(ctx, self, "say which shell: zsh or fish");
	return 0;
}

// What the completion scripts ask for while the person types, one word a
// line; a ref has its title after a tab, and a title its ref. It must never
// get in the way: it only looks at the lists, and when anything is wrong (no
// lists yet, a file it cannot read, a word it does not know) it prints
// nothing and still exits with 0.
int runComplete(const command&, const args::parsed& line, cliContext& ctx) {
	std::string what = line.operands.empty() ? "" : line.operands[0];
	if (what == "statuses")
		for (const auto& word : statusWords())
			ctx.out << word << "\n";
	if (what == "fields")
		for (const auto& name : settableNames())
			ctx.out << name << "=\n";
	if ((what != "refs" and what != "titles") or not ctx.db.peek())
		return 0;
	for (mediaType type : {mediaType::kAnime, mediaType::kManga}) {
		for (const auto& e : ctx.db.getEntries(type))
			ctx.out << (what == "refs" ? output::tsvLine({refs::of(e), e.title})
			                           : output::tsvLine({e.title, refs::of(e)}));
	}
	return 0;
}

// The build's own command: the manual page is docs/catalist.1.in with the
// version and the commands of the table put in.
int runManual(const command&, const args::parsed&, cliContext& ctx) {
	std::string page{std::istreambuf_iterator<char>(ctx.in), std::istreambuf_iterator<char>()};
	const std::map<std::string, std::string> filled = {
	    {"@PROJECT_VERSION@", kVersionString}, {"@COMMANDS@\n", completion::manual(commands())}};
	for (const auto& [mark, text] : filled) {
		auto at = page.find(mark);
		if (at != std::string::npos)
			page.replace(at, mark.size(), text);
	}
	ctx.out << page;
	return 0;
}

}  // namespace

std::vector<command> setupCommands() {
	return {
	    {.name = "provider",
	     .summary = "Show or choose your provider",
	     .details = "Your provider is the site your titles are looked at on: AniList (the\n"
	                "default), MyAnimeList or Kitsu. 'search', 'add' and 'refresh' ask it\n"
	                "unless --from names another site, and 'show' gives a title's page\n"
	                "there. Without an operand the provider is printed, as anilist, mal or\n"
	                "kitsu. Choosing a provider is not a step for undo.\n",
	     .operands = "[anilist|mal|kitsu]",
	     .run = runProvider,
	     .completes = operandKind::kWords,
	     .words = {"anilist", "mal", "kitsu"}},
	    {.name = "completion",
	     .summary = "Print the completion script of a shell",
	     .details = "An installed Catalist has put the scripts where the shells look for\n"
	                "them. To set one up by hand:\n"
	                "  catalist completion zsh > ~/.zfunc/_catalist\n"
	                "  catalist completion fish > ~/.config/fish/completions/catalist.fish\n"
	                "For zsh the directory must be in $fpath before compinit runs. The\n"
	                "scripts complete the commands and their options, and your titles by\n"
	                "ref or by any part of their name.\n",
	     .operands = "zsh|fish",
	     .run = runCompletion,
	     .completes = operandKind::kWords,
	     .words = {"zsh", "fish"}},
	    {.name = "__complete",
	     .summary = "Print the words the completion scripts ask for",
	     .operands = "refs|titles|statuses|fields",
	     .run = runComplete,
	     .hidden = true},
	    {.name = "__man",
	     .summary = "Fill in the manual page read from standard input",
	     .run = runManual,
	     .hidden = true},
	};
}

}  // namespace catalist::cli
