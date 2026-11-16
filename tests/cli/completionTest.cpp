#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "cli/cli.hpp"
#include "cli/completion.hpp"

namespace completion = catalist::completion;
using catalist::command;
using catalist::operandKind;

namespace {

bool has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

int runNothing(const command&, const catalist::args::parsed&, catalist::cliContext&) {
	return 0;
}

// A table of the kinds the scripts know, with text that needs quoting.
std::vector<command> sampleTable() {
	return {
	    {.name = "ls",
	     .summary = "List your titles",
	     .details = "Lists them.\n\nAs lines:\n  ref title\nThat is all.\n",
	     .options = {{'s', "", "STATUS", "only one [status]", {"watching", "on-hold"}},
	                 {0, "sort", "KEY", "the order", {"new", "title"}},
	                 {'q', "", "", "print the refs"}},
	     .operands = "[QUERY]",
	     .run = runNothing,
	     .completes = operandKind::kTitle},
	    {.name = "+",
	     .aliases = {"inc"},
	     .summary = "Count one more",
	     .options = {{'n', "", "N", "count N, not one"}},
	     .operands = "REF...",
	     .run = runNothing,
	     .completes = operandKind::kRefs},
	    {.name = "set",
	     .summary = "Set the person's fields",
	     .operands = "REF FIELD=VALUE...",
	     .run = runNothing,
	     .completes = operandKind::kRefThenFields},
	    {.name = "import",
	     .summary = "Add titles",
	     .operands = "FILE...",
	     .run = runNothing,
	     .completes = operandKind::kFiles},
	    {.name = "completion",
	     .summary = "Print a script",
	     .operands = "zsh|fish",
	     .run = runNothing,
	     .completes = operandKind::kWords,
	     .words = {"zsh", "fish"}},
	    {.name = "help",
	     .summary = "Show help",
	     .operands = "[COMMAND]",
	     .run = runNothing,
	     .completes = operandKind::kCommands},
	    {.name = "__secret", .summary = "Never shown", .run = runNothing, .hidden = true},
	};
}

}  // namespace

TEST_CASE("completion zsh serves catalist, from the table") {
	std::string script = completion::zsh(sampleTable());
	CHECK(script.rfind("#compdef catalist\n", 0) == 0);
	CHECK(has(script, "\t\t'ls:List your titles'\n"));
	CHECK(has(script, "\t\t'set:Set the person'\\''s fields'\n"));
	CHECK_FALSE(has(script, "__secret"));
	// Options, their values and what to choose from.
	CHECK(has(script, "'-s+[only one \\[status\\]]:status:(watching on-hold)'"));
	CHECK(has(script, "'--sort=[the order]:key:(new title)'"));
	CHECK(has(script, "'-n+[count N, not one]:n: '"));
	CHECK(has(script, "'-q[print the refs]'"));
	CHECK(has(script, "'(-h --help)-h[show this help]'"));
	// Each command by all its names; a name that is not a word is quoted.
	CHECK(has(script, "\t'+'|inc)\n"));
	CHECK(has(script, "'1:title:_catalist_titles'"));
	CHECK(has(script, "'*:title:_catalist_named'"));
	CHECK(has(script, "'1:title:_catalist_named' \\\n\t\t\t'*:field:_catalist_fields'"));
	CHECK(has(script, "'*:file:_files'"));
	CHECK(has(script, "'1:choice:(zsh fish)'"));
	CHECK(has(script, "'1:command:_catalist_commands'"));
	// What only the lists know is asked of the program it completes.
	CHECK(has(script, "$_catalist_program __complete $1 2>/dev/null"));
	CHECK(has(script, "\tcompdef _catalist catalist\n"));
}

TEST_CASE("completion fish completes the commands, their options and the lists") {
	std::string script = completion::fish(sampleTable());
	CHECK(has(script, "complete -c catalist -f\n"));
	CHECK(has(script,
	          "complete -c catalist -n '__catalist_no_command' -a 'ls' -d 'List your titles'\n"));
	CHECK(has(script, "-a 'set' -d 'Set the person\\'s fields'\n"));
	CHECK_FALSE(has(script, "__secret"));
	CHECK(has(script,
	          "complete -c catalist -n '__catalist_command ls' -s s -x -a 'watching on-hold' "
	          "-d 'only one [status]'\n"));
	CHECK(has(script, "-n '__catalist_command ls' -l sort -x -a 'new title' -d 'the order'\n"));
	CHECK(has(script, "-n '__catalist_command ls' -s q -d 'print the refs'\n"));
	CHECK(has(script, "-n '__catalist_command ls' -s h -l help -d 'show this help'\n"));
	CHECK(has(script, "-n '__catalist_command + inc' -k -a '(__catalist_named)'\n"));
	CHECK(has(script,
	          "-n '__catalist_command set; and test (__catalist_operands) -ge 1' "
	          "-a '(__catalist_fields)'\n"));
	CHECK(has(script, "-n '__catalist_command import' -F\n"));
	CHECK(has(script,
	          "-n '__catalist_command completion; and test (__catalist_operands) -eq 0' "
	          "-a 'zsh fish'\n"));
	CHECK(has(script,
	          "-n '__catalist_command help; and test (__catalist_operands) -eq 0' "
	          "-a 'ls' -d 'List your titles'\n"));
}

TEST_CASE("completion scripts know the program's own options, before the command") {
	std::string zsh = completion::zsh(sampleTable());
	CHECK(has(zsh,
	          "'--color=[colours: auto (at a terminal), always or never]:when:(auto always "
	          "never)'"));
	CHECK(has(zsh, "'(-t --table)-t[draw tables into a pipe as well ($COLUMNS wide)]'"));
	CHECK(has(zsh, "'(- *)-V[print the version]'"));
	std::string fish = completion::fish(sampleTable());
	CHECK(has(fish,
	          "complete -c catalist -n __catalist_no_command -l color -x -a 'auto always "
	          "never'"));
	CHECK(has(fish, "complete -c catalist -n __catalist_no_command -s t -l table"));
	// The command is the first word after them; --color may have its value
	// apart.
	CHECK(has(fish, "if contains -- $words[1] --color\n"));
}

TEST_CASE("completion manual sets each command in roff") {
	std::string page = completion::manual(sampleTable());
	CHECK(page.rfind(".TP\n\\fBls\\fR [\\fB\\-q\\fR] [\\fB\\-s\\fR \\fISTATUS\\fR] "
	                 "[\\fB\\-\\-sort\\fR=\\fIKEY\\fR] [\\fIQUERY\\fR]\nList your titles.\n",
	                 0) == 0);
	// Paragraphs, and the indented lines as they are written.
	CHECK(has(page, ".IP\nLists them.\n.IP\nAs lines:\n.nf\nref title\n.fi\nThat is all.\n"));
	CHECK(has(page, ".RS\n.TP\n\\fB\\-s\\fR \\fISTATUS\\fR\nonly one [status]\n"));
	CHECK(has(page,
	          ".TP\n\\fB+\\fR [\\fB\\-n\\fR \\fIN\\fR] \\fIREF\\fR...\n"
	          "Count one more.\nAlso \\fBinc\\fR.\n"));
	CHECK(has(page, "\\fIFIELD\\fR=\\fIVALUE\\fR..."));
	CHECK(has(page, "\\fBzsh\\fR|\\fBfish\\fR"));
	CHECK_FALSE(has(page, "secret"));
}
