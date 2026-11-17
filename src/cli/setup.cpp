#include <format>
#include <ostream>

#include "cli/common.hpp"
#include "cli/completion.hpp"
#include "cli/refs.hpp"

namespace catalist::cli {

namespace {

int RunProvider(const Command& self, const args::Parsed& line, CliContext& ctx) {
	if (line.operands.size() > 1)
		return UsageError(ctx, self, "name one provider: anilist, mal or kitsu");
	if (line.operands.empty()) {
		if (not Load(ctx))
			return 1;
		ctx.out << ProviderKey(ctx.db.provider()) << "\n";
		return 0;
	}
	auto chosen = ParseProvider(line.operands[0]);
	if (not chosen)
		return UsageError(ctx, self,
		                  std::format("unknown provider '{}'{}; choose anilist, mal or kitsu",
		                              line.operands[0], DidYouMean(line.operands[0], self.words)));
	if (not LoadForWrite(ctx))
		return 1;
	if (ctx.db.provider() == *chosen) {
		ctx.err << "Nothing changed.\n";
		return 0;
	}
	ctx.db.set_provider(*chosen);
	if (not ctx.db.SaveSettings())
		return SaveFailed(ctx);
	ctx.err << std::format("Your provider is now {}.\n", ProviderName(*chosen));
	return 0;
}

int RunCompletion(const Command& self, const args::Parsed& line, CliContext& ctx) {
	std::string shell = line.operands.size() == 1 ? line.operands[0] : "";
	if (shell == "zsh")
		ctx.out << completion::Zsh(Commands());
	else if (shell == "fish")
		ctx.out << completion::Fish(Commands());
	else
		return UsageError(ctx, self, "say which shell: zsh or fish");
	return 0;
}

// What the completion scripts ask for while the person types, one word a
// line; a ref has its title after a tab, and a title its ref. It must never
// get in the way: it only looks at the lists, and when anything is wrong (no
// lists yet, a file it cannot read, a word it does not know) it prints
// nothing and still exits with 0.
int RunComplete(const Command&, const args::Parsed& line, CliContext& ctx) {
	std::string what = line.operands.empty() ? "" : line.operands[0];
	if (what == "statuses")
		for (const auto& word : StatusWords())
			ctx.out << word << "\n";
	if (what == "fields")
		for (const auto& name : SettableNames())
			ctx.out << name << "=\n";
	if ((what != "refs" and what != "titles") or not ctx.db.Peek())
		return 0;
	for (MediaType type : {MediaType::kAnime, MediaType::kManga}) {
		for (const auto& e : ctx.db.GetEntries(type))
			ctx.out << (what == "refs" ? output::TsvLine({refs::Of(e), e.title})
			                           : output::TsvLine({e.title, refs::Of(e)}));
	}
	return 0;
}

// The build's own command: the COMMANDS section of the manual page, which
// cmake/shell_support.cmake puts into docs/catalist.1.in.
int RunManual(const Command&, const args::Parsed&, CliContext& ctx) {
	ctx.out << completion::Manual(Commands());
	return 0;
}

// The build's own command: the command list of README.md, which
// cmake/readme.cmake puts between its markers.
int RunReadme(const Command&, const args::Parsed&, CliContext& ctx) {
	ctx.out << completion::Readme(Commands());
	return 0;
}

}  // namespace

std::vector<Command> SetupCommands() {
	return {
	    {.name = "provider",
	     .summary = "Show or choose your provider",
	     .details = "Your provider is the site your titles are looked at on: AniList (the\n"
	                "default), MyAnimeList or Kitsu. 'search', 'add', 'refresh' and\n"
	                "'import --user' ask it unless --from names another site, and 'show'\n"
	                "gives a title's page there. Without an operand the provider is\n"
	                "printed, as anilist, mal or kitsu. Choosing a provider is not a step\n"
	                "for undo.\n",
	     .operands = "[anilist|mal|kitsu]",
	     .run = RunProvider,
	     .completes = OperandKind::kWords,
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
	     .run = RunCompletion,
	     .completes = OperandKind::kWords,
	     .words = {"zsh", "fish"}},
	    {.name = "__complete",
	     .summary = "Print the words the completion scripts ask for",
	     .operands = "refs|titles|statuses|fields",
	     .run = RunComplete,
	     .hidden = true},
	    {.name = "__man",
	     .summary = "Print the commands as the manual page lists them",
	     .run = RunManual,
	     .hidden = true},
	    {.name = "__readme",
	     .summary = "Print the commands as README.md lists them",
	     .run = RunReadme,
	     .hidden = true},
	};
}

}  // namespace catalist::cli
