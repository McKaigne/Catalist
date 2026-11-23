#include "cli/completion.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "cli/cli.hpp"

namespace completion = catalist::completion;
using catalist::Command;
using catalist::OperandKind;

namespace {

bool Has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}

int RunNothing(const Command&, const catalist::args::Parsed&, catalist::CliContext&) {
	return 0;
}

// A table of the kinds the scripts know, with text that needs quoting.
std::vector<Command> SampleTable() {
	return {
	    {.name = "ls",
	     .summary = "List your titles",
	     .details = "Lists them.\n\nAs lines:\n  ref title\nThat is all.\n",
	     .options = {{'s', "", "STATUS", "only one [status]", {"watching", "on-hold"}},
	                 {0, "sort", "KEY", "the order", {"new", "title"}},
	                 {'q', "", "", "print the refs"}},
	     .operands = "[QUERY]",
	     .run = RunNothing,
	     .completes = OperandKind::kTitle},
	    {.name = "+",
	     .aliases = {"inc"},
	     .summary = "Count one more",
	     .options = {{'n', "", "N", "count N, not one"}},
	     .operands = "REF...",
	     .run = RunNothing,
	     .completes = OperandKind::kRefs},
	    {.name = "set",
	     .summary = "Set the person's fields",
	     .operands = "REF FIELD=VALUE...",
	     .run = RunNothing,
	     .completes = OperandKind::kRefThenFields},
	    {.name = "import",
	     .summary = "Add titles",
	     .operands = "FILE...",
	     .run = RunNothing,
	     .completes = OperandKind::kFiles},
	    {.name = "completion",
	     .summary = "Print a script",
	     .operands = "zsh|fish",
	     .run = RunNothing,
	     .completes = OperandKind::kWords,
	     .words = {"zsh", "fish"}},
	    {.name = "help",
	     .summary = "Show help",
	     .operands = "[COMMAND]",
	     .run = RunNothing,
	     .completes = OperandKind::kCommands},
	    {.name = "__secret", .summary = "Never shown", .run = RunNothing, .hidden = true},
	};
}

}  // namespace

TEST_CASE("completion zsh serves catalist, from the table") {
	std::string script = completion::Zsh(SampleTable());
	CHECK(script.rfind("#compdef catalist\n", 0) == 0);
	CHECK(Has(script, "\t\t'ls:List your titles'\n"));
	CHECK(Has(script, "\t\t'set:Set the person'\\''s fields'\n"));
	CHECK_FALSE(Has(script, "__secret"));
	// Options, their values and what to choose from.
	CHECK(Has(script, "'-s+[only one \\[status\\]]:status:(watching on-hold)'"));
	CHECK(Has(script, "'--sort=[the order]:key:(new title)'"));
	CHECK(Has(script, "'-n+[count N, not one]:n: '"));
	CHECK(Has(script, "'-q[print the refs]'"));
	CHECK(Has(script, "'(-h --help)-h[show this help]'"));
	// Each Command by all its names; a name that is not a word is quoted.
	CHECK(Has(script, "\t'+'|inc)\n"));
	CHECK(Has(script, "'1:title:_catalist_titles'"));
	CHECK(Has(script, "'*:title:_catalist_named'"));
	CHECK(Has(script, "'1:title:_catalist_named' \\\n\t\t\t'*:field:_catalist_fields'"));
	CHECK(Has(script, "'*:file:_files'"));
	CHECK(Has(script, "'1:choice:(zsh fish)'"));
	CHECK(Has(script, "'1:command:_catalist_commands'"));
	// What only the lists know is asked of the program it completes.
	CHECK(Has(script, "$_catalist_program __complete $1 2>/dev/null"));
	CHECK(Has(script, "\tcompdef _catalist catalist\n"));
}

TEST_CASE("completion fish completes the commands, their options and the lists") {
	std::string script = completion::Fish(SampleTable());
	CHECK(Has(script, "complete -c catalist -f\n"));
	CHECK(Has(script,
	          "complete -c catalist -n '__catalist_no_command' -a 'ls' -d 'List your titles'\n"));
	CHECK(Has(script, "-a 'set' -d 'Set the person\\'s fields'\n"));
	CHECK_FALSE(Has(script, "__secret"));
	CHECK(Has(script,
	          "complete -c catalist -n '__catalist_command ls' -s s -x -a 'watching on-hold' "
	          "-d 'only one [status]'\n"));
	CHECK(Has(script, "-n '__catalist_command ls' -l sort -x -a 'new title' -d 'the order'\n"));
	CHECK(Has(script, "-n '__catalist_command ls' -s q -d 'print the refs'\n"));
	CHECK(Has(script, "-n '__catalist_command ls' -s h -l help -d 'show this help'\n"));
	CHECK(Has(script, "-n '__catalist_command + inc' -k -a '(__catalist_named)'\n"));
	CHECK(Has(script,
	          "-n '__catalist_command set; and test (__catalist_operands) -ge 1' "
	          "-a '(__catalist_fields)'\n"));
	CHECK(Has(script, "-n '__catalist_command import' -F\n"));
	CHECK(Has(script,
	          "-n '__catalist_command completion; and test (__catalist_operands) -eq 0' "
	          "-a 'zsh fish'\n"));
	CHECK(Has(script,
	          "-n '__catalist_command help; and test (__catalist_operands) -eq 0' "
	          "-a 'ls' -d 'List your titles'\n"));
}

TEST_CASE("completion scripts know the program's own options, before the command") {
	std::string zsh = completion::Zsh(SampleTable());
	CHECK(Has(zsh,
	          "'--color=[colours: auto (at a terminal), always or never]:when:(auto always "
	          "never)'"));
	CHECK(Has(zsh, "'(-t --table)-t[draw tables into a pipe as well ($COLUMNS wide)]'"));
	CHECK(Has(zsh, "'(- *)-V[print the version]'"));
	std::string fish = completion::Fish(SampleTable());
	CHECK(Has(fish,
	          "complete -c catalist -n __catalist_no_command -l color -x -a 'auto always "
	          "never'"));
	CHECK(Has(fish, "complete -c catalist -n __catalist_no_command -s t -l table"));
	// The Command is the first word after them; --color may have its value
	// apart.
	CHECK(Has(fish, "if contains -- $words[1] --color\n"));
}

TEST_CASE("completion manual sets each command in roff") {
	std::string page = completion::Manual(SampleTable());
	CHECK(page.rfind(".TP\n\\fBls\\fR [\\fB\\-q\\fR] [\\fB\\-s\\fR \\fISTATUS\\fR] "
	                 "[\\fB\\-\\-sort\\fR=\\fIKEY\\fR] [\\fIQUERY\\fR]\nList your titles.\n",
	                 0) == 0);
	// Paragraphs, and the indented lines as they are written.
	CHECK(Has(page, ".IP\nLists them.\n.IP\nAs lines:\n.nf\nref title\n.fi\nThat is all.\n"));
	CHECK(Has(page, ".RS\n.TP\n\\fB\\-s\\fR \\fISTATUS\\fR\nonly one [status]\n"));
	CHECK(Has(page,
	          ".TP\n\\fB+\\fR [\\fB\\-n\\fR \\fIN\\fR] \\fIREF\\fR...\n"
	          "Count one more.\nAlso \\fBinc\\fR.\n"));
	CHECK(Has(page, "\\fIFIELD\\fR=\\fIVALUE\\fR..."));
	CHECK(Has(page, "\\fBzsh\\fR|\\fBfish\\fR"));
	CHECK_FALSE(Has(page, "secret"));
}

TEST_CASE("completion readme lists each command in Org") {
	std::string list = completion::Readme(SampleTable());
	CHECK(list.rfind("- ~catalist ls [-q] [-s STATUS] [--sort=KEY] [QUERY]~: List your titles.\n",
	                 0) == 0);
	CHECK(Has(list, "- ~catalist + [-n N] REF...~: Count one more. Also ~inc~.\n"));
	CHECK(Has(list, "- ~catalist completion zsh|fish~: Print a script.\n"));
	// The program's own options follow, their help as it is.
	CHECK(Has(list, "\nOptions, before the command:\n\n- ~--color=WHEN~: colours: "));
	CHECK(Has(list, "- ~-V, --version~: print the version\n"));
	CHECK_FALSE(Has(list, "secret"));
}
