#include <fmt/format.h>

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
		                  fmt::format("unknown provider '{}'{}; choose anilist, mal or kitsu",
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
	ctx.err << fmt::format("Your provider is now {}.\n", providerName(*chosen));
	return 0;
}

int runCompletion(const command& self, const args::parsed& line, cliContext& ctx) {
	std::string shell = line.operands.size() == 1 ? line.operands[0] : "";
	if (shell == "zsh")
		ctx.out << completion::zsh(commands());
	else if (shell == "fish")
		ctx.out << completion::fish(commands());
	else if (shell == "bash")
		ctx.out << completion::bash(commands());
	else
		return usageError(ctx, self, "say which shell: zsh, fish or bash");
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
	    {"provider",
	     {},
	     "Show or choose your provider",
	     "Your provider is the site your titles are looked at on: AniList (the\n"
	     "default), MyAnimeList or Kitsu. 'search', 'add' and 'refresh' ask it\n"
	     "unless --from names another site, and 'show' gives a title's page\n"
	     "there. Without an operand the provider is printed, as anilist, mal or\n"
	     "kitsu. Choosing a provider is not a step for undo.\n",
	     {},
	     "[anilist|mal|kitsu]",
	     runProvider,
	     operandKind::kWords,
	     {"anilist", "mal", "kitsu"}},
	    {"completion",
	     {},
	     "Print the completion script of a shell",
	     "An installed Catalist has put the scripts where the shells look for\n"
	     "them. To set one up by hand:\n"
	     "  catalist completion zsh > ~/.zfunc/_catalist\n"
	     "  catalist completion fish > ~/.config/fish/completions/catalist.fish\n"
	     "  catalist completion bash > ~/.local/share/bash-completion/completions/catalist\n"
	     "For zsh the directory must be in $fpath before compinit runs. Without\n"
	     "bash-completion, source the bash script from ~/.bashrc. The scripts\n"
	     "complete the commands and their options, and your titles by ref or by\n"
	     "their name (in bash, its start; in zsh and fish, any part of it).\n",
	     {},
	     "zsh|fish|bash",
	     runCompletion,
	     operandKind::kWords,
	     {"zsh", "fish", "bash"}},
	    {"__complete",
	     {},
	     "Print the words the completion scripts ask for",
	     "",
	     {},
	     "refs|titles|statuses|fields",
	     runComplete,
	     operandKind::kNone,
	     {},
	     true},
	    {"__man",
	     {},
	     "Fill in the manual page read from standard input",
	     "",
	     {},
	     "",
	     runManual,
	     operandKind::kNone,
	     {},
	     true},
	};
}

}  // namespace catalist::cli
