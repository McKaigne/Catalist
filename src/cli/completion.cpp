#include "cli/completion.hpp"

#include <algorithm>
#include <cctype>
#include <format>

#include "core/text.hpp"

namespace catalist::completion {

namespace {

std::string Joined(const std::vector<std::string>& words, const std::string& between) {
	std::string all;
	for (const auto& word : words)
		all += (all.empty() ? "" : between) + word;
	return all;
}

// Every `from` in the text written as `to`.
std::string Replaced(std::string text, const std::string& from, const std::string& to) {
	for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		text.replace(at, from.size(), to);
	return text;
}

// The command's name and the other names it goes by.
std::vector<std::string> NamesOf(const Command& of) {
	std::vector<std::string> names = {of.name};
	names.insert(names.end(), of.aliases.begin(), of.aliases.end());
	return names;
}

// The program's own options that take a value, as written with the value
// apart ("--color always"): the scripts skip that value with them.
std::vector<std::string> GlobalsApart() {
	std::vector<std::string> spellings;
	for (const auto& each : GlobalOptions()) {
		if (each.value.empty())
			continue;
		if (each.letter)
			spellings.push_back(std::format("-{}", each.letter));
		if (not each.name.empty())
			spellings.push_back("--" + each.name);
	}
	return spellings;
}

std::vector<const Command*> ShownOf(const std::vector<Command>& table) {
	std::vector<const Command*> shown;
	for (const auto& each : table)
		if (not each.hidden)
			shown.push_back(&each);
	return shown;
}

// "REF...", "[QUERY]" and "FIELD=VALUE" as the word for what is asked for:
// "ref", "query", "field". Words to choose from, as "zsh|fish", are a choice.
std::string WordFor(const std::string& operands) {
	if (operands.find('|') != std::string::npos)
		return "choice";
	std::string word;
	for (char c : operands) {
		if (not std::isalpha(static_cast<unsigned char>(c)) and not word.empty())
			break;
		if (std::isalpha(static_cast<unsigned char>(c)))
			word += c;
	}
	return utf8::Lowercase(word);
}

// ---- zsh ----

std::string ZshQuoted(const std::string& text) {
	return std::format("'{}'", Replaced(text, "'", "'\\''"));
}

// The words in the brackets of an option: a bracket in them would end them.
std::string ZshExplained(const std::string& help) {
	std::string safe = Replaced(help, "\\", "\\\\");
	safe = Replaced(safe, "[", "\\[");
	return Replaced(safe, "]", "\\]");
}

// What follows an option's name: its explanation and, when it takes a
// value, what the value is and the values to choose from.
std::string ZshOptionTail(const args::Option& taken) {
	std::string tail = std::format("[{}]", ZshExplained(taken.help));
	if (taken.value.empty())
		return tail;
	std::string choices =
	    taken.choices.empty() ? " " : std::format("({})", Joined(taken.choices, " "));
	return tail + std::format(":{}:{}", utf8::Lowercase(taken.value), choices);
}

// One spec of _arguments for each way the option is written. The two ways
// of one option rule each other out.
std::vector<std::string> ZshOptionSpecs(const args::Option& taken) {
	std::string tail = ZshOptionTail(taken);
	std::string once;
	if (taken.letter and not taken.name.empty())
		once = std::format("(-{} --{})", taken.letter, taken.name);
	std::vector<std::string> specs;
	if (taken.letter)
		specs.push_back(
		    std::format("{}-{}{}{}", once, taken.letter, taken.value.empty() ? "" : "+", tail));
	if (not taken.name.empty())
		specs.push_back(
		    std::format("{}--{}{}{}", once, taken.name, taken.value.empty() ? "" : "=", tail));
	return specs;
}

std::vector<std::string> ZshOperandSpecs(const Command& of) {
	std::string word = WordFor(of.operands);
	switch (of.completes) {
		case OperandKind::kNone:
			return {};
		case OperandKind::kRefs:
			return {"*:title:_catalist_named"};
		case OperandKind::kRefThenFields:
			return {"1:title:_catalist_named", "*:field:_catalist_fields"};
		case OperandKind::kTitle:
			return {"1:title:_catalist_titles"};
		case OperandKind::kText:
			return {std::format("1:{}: ", word)};
		case OperandKind::kFiles:
			return {"*:file:_files"};
		case OperandKind::kWords:
			return {std::format("1:{}:({})", word, Joined(of.words, " "))};
		case OperandKind::kCommands:
			return {"1:command:_catalist_commands"};
	}
	return {};
}

// "ls)" or "'+'|inc)": the case that takes the command by any of its names.
std::string ZshCase(const Command& of) {
	std::vector<std::string> names;
	for (const auto& name : NamesOf(of))
		names.push_back(std::isalpha(static_cast<unsigned char>(name[0])) ? name : ZshQuoted(name));
	return Joined(names, "|") + ")";
}

std::string ZshCommand(const Command& of) {
	std::vector<std::string> specs;
	std::vector<args::Option> options = of.options;
	options.push_back(HelpOption());
	for (const auto& option : options)
		for (const auto& spec : ZshOptionSpecs(option))
			specs.push_back(spec);
	for (const auto& spec : ZshOperandSpecs(of))
		specs.push_back(spec);

	std::string text = std::format("\t{}\n\t\t_arguments -s -S -A '-*' :", ZshCase(of));
	for (const auto& spec : specs)
		text += std::format(" \\\n\t\t\t{}", ZshQuoted(spec));
	return text + " && ret=0\n\t\t;;\n";
}

// The helpers ask the program by the name it was typed under, so a program
// outside PATH completes from its own lists.
const char* const kZshHelpers = R"zsh(_catalist_ask() {
	_call_program catalist $_catalist_program __complete $1 2>/dev/null
}

# The refs of the lists, each with its title beside it.
_catalist_refs() {
	local -a lines refs
	local line
	lines=(${(f)"$(_catalist_ask refs)"})
	for line in $lines; do
		refs+=("${${line%%$'\t'*}//:/\\:}:${line#*$'\t'}")
	done
	_describe -t refs ref refs
}

# The titles: any part of one completes it, in capitals or not.
_catalist_titles() {
	local -a lines titles
	local expl
	lines=(${(f)"$(_catalist_ask titles)"})
	titles=(${lines%%$'\t'*})
	_wanted titles expl title compadd -M 'm:{[:lower:]}={[:upper:]} l:|=* r:|=*' -a titles
}

_catalist_named() {
	_alternative 'refs:ref:_catalist_refs' 'titles:title:_catalist_titles'
}

_catalist_statuses() {
	local -a statuses
	local expl
	statuses=(${(f)"$(_catalist_ask statuses)"})
	_wanted statuses expl status compadd -a statuses
}

# FIELD=VALUE: the fields, and after status= the statuses.
_catalist_fields() {
	if compset -P 'status='; then
		_catalist_statuses
		return
	fi
	local -a fields
	local expl
	fields=(${(f)"$(_catalist_ask fields)"})
	_wanted fields expl field compadd -S '' -a fields
}

_catalist_commands() {
	_describe -t commands command _catalist_names
}
)zsh";

// ---- fish ----

std::string FishQuoted(const std::string& text) {
	return std::format("'{}'", Replaced(Replaced(text, "\\", "\\\\"), "'", "\\'"));
}

const char* const kFishHelpers = R"fish(# Asks the program, by the name it was typed under.
function __catalist_ask
	set -l words (commandline -opc)
	command $words[1] __complete $argv 2>/dev/null
end

# The words typed after the program's own options: the command, then its
# operands and options.
function __catalist_after_options
	set -l words (commandline -opc)
	set -e words[1]
	while set -q words[1]; and string match -qr -- '^-.' $words[1]
		# An option of the program that takes its value apart.
		if contains -- $words[1] @VALUED@
			set -e words[1]
		end
		set -e words[1]
	end
	string join \n -- $words
end

function __catalist_no_command
	set -l words (__catalist_after_options)
	not set -q words[1]
end

# Whether the command typed is one of the names given.
function __catalist_command
	set -l words (__catalist_after_options)
	set -q words[1]; and contains -- $words[1] $argv
end

# How many operands the command has so far.
function __catalist_operands
	set -l words (__catalist_after_options)
	set -e words[1]
	count (string match -rv -- '^-.' $words)
end

# The refs with their titles, then the titles with their refs.
function __catalist_named
	__catalist_ask refs
	__catalist_ask titles
end

# FIELD=VALUE: the fields, and after status= the statuses.
function __catalist_fields
	if string match -q 'status=*' -- (commandline -ct)
		printf 'status=%s\n' (__catalist_ask statuses)
	else
		__catalist_ask fields
	end
end
)fish";

std::string FishOption(const args::Option& taken) {
	std::string text;
	if (taken.letter)
		text += std::format(" -s {}", taken.letter);
	if (not taken.name.empty())
		text += std::format(" -l {}", taken.name);
	if (not taken.value.empty())
		text += " -x";
	if (not taken.choices.empty())
		text += std::format(" -a {}", FishQuoted(Joined(taken.choices, " ")));
	return text + std::format(" -d {}", FishQuoted(taken.help));
}

// What `complete` is given for the command's operands, each with the
// condition (beyond the command itself) under which it holds.
std::vector<std::pair<std::string, std::string>> FishOperands(const Command& of) {
	const std::string first = "; and test (__catalist_operands) -eq 0";
	const std::string later = "; and test (__catalist_operands) -ge 1";
	switch (of.completes) {
		case OperandKind::kNone:
		case OperandKind::kText:
		case OperandKind::kCommands:
			return {};
		case OperandKind::kRefs:
			return {{"", "-k -a '(__catalist_named)'"}};
		case OperandKind::kRefThenFields:
			return {{first, "-k -a '(__catalist_named)'"}, {later, "-a '(__catalist_fields)'"}};
		case OperandKind::kTitle:
			return {{first, "-a '(__catalist_ask titles)'"}};
		case OperandKind::kFiles:
			return {{"", "-F"}};
		case OperandKind::kWords:
			return {{first, std::format("-a {}", FishQuoted(Joined(of.words, " ")))}};
	}
	return {};
}

// The lines that offer the commands themselves, under `condition`.
std::string FishCommandNames(const std::vector<Command>& table, const std::string& condition) {
	std::string text;
	for (const Command* each : ShownOf(table))
		text += std::format("complete -c catalist -n {} -a {} -d {}\n", FishQuoted(condition),
		                    FishQuoted(each->name), FishQuoted(each->summary));
	return text;
}

std::string FishCommand(const std::vector<Command>& table, const Command& of) {
	std::string condition = std::format("__catalist_command {}", Joined(NamesOf(of), " "));
	std::string text;
	std::vector<args::Option> options = of.options;
	options.push_back(HelpOption());
	for (const auto& option : options)
		text += std::format("complete -c catalist -n {}{}\n", FishQuoted(condition),
		                    FishOption(option));
	for (const auto& [when, what] : FishOperands(of))
		text += std::format("complete -c catalist -n {} {}\n", FishQuoted(condition + when), what);
	if (of.completes == OperandKind::kCommands)
		text += FishCommandNames(table, condition + "; and test (__catalist_operands) -eq 0");
	return text;
}

// ---- The manual page ----

// Text as roff prints it: a backslash and a hyphen-minus are written out,
// and a line cannot begin with roff's control characters.
std::string RoffText(const std::string& text) {
	std::string safe = Replaced(Replaced(text, "\\", "\\e"), "-", "\\-");
	if (not safe.empty() and (safe[0] == '.' or safe[0] == '\''))
		safe = "\\&" + safe;
	return safe;
}

bool IsSyntax(char c) {
	return std::string(" []|=.,").find(c) != std::string::npos;
}

// A usage line or an option in the manual's type: words in capitals stand
// for something and are italic, the rest is typed as written and is bold.
std::string RoffSyntax(const std::string& line) {
	std::string styled;
	for (size_t at = 0; at < line.size();) {
		if (IsSyntax(line[at])) {
			styled += line[at++];
			continue;
		}
		size_t end = at;
		while (end < line.size() and not IsSyntax(line[end]))
			++end;
		std::string word = line.substr(at, end - at);
		bool stands_for = std::none_of(word.begin(), word.end(), [](unsigned char c) {
			return std::islower(c) or c == '-' or c == '+';
		});
		styled += std::format("\\f{}{}\\fR", stands_for ? 'I' : 'B', RoffText(word));
		at = end;
	}
	return styled;
}

// "-s STATUS", "--sort=KEY", "-h, --help".
std::string OptionSyntax(const args::Option& taken) {
	std::string label = taken.letter ? std::format("-{}", taken.letter) : "";
	if (not taken.name.empty())
		label += std::format("{}--{}", label.empty() ? "" : ", ", taken.name);
	if (not taken.value.empty())
		label += std::format("{}{}", taken.name.empty() ? " " : "=", taken.value);
	return label;
}

// A command's `details` under its entry: paragraphs, with the indented
// lines (examples, column names) set as they are written.
std::string RoffDetails(const std::string& details) {
	std::string text;
	bool as_written = false;
	bool new_paragraph = true;
	size_t at = 0;
	while (at < details.size()) {
		size_t end = details.find('\n', at);
		std::string line = details.substr(at, end == std::string::npos ? end : end - at);
		at = end == std::string::npos ? details.size() : end + 1;
		if (line.empty()) {
			new_paragraph = true;
			continue;
		}
		bool indented = line[0] == ' ';
		if (as_written and not indented)
			text += ".fi\n";
		if (new_paragraph)
			text += ".IP\n";
		if (indented and not as_written)
			text += ".nf\n";
		as_written = indented;
		new_paragraph = false;
		text += RoffText(indented ? line.substr(line.find_first_not_of(' ')) : line) + "\n";
	}
	return text + (as_written ? ".fi\n" : "");
}

std::string RoffCommand(const Command& of) {
	std::string text =
	    std::format(".TP\n{}\n{}.\n", RoffSyntax(UsageLine(of)), RoffText(of.summary));
	if (not of.aliases.empty()) {
		std::vector<std::string> also;
		for (const auto& alias : of.aliases)
			also.push_back(std::format("\\fB{}\\fR", RoffText(alias)));
		text += std::format("Also {}.\n", Joined(also, ", "));
	}
	text += RoffDetails(of.details);
	if (of.options.empty())
		return text;
	text += ".RS\n";
	for (const auto& option : of.options)
		text +=
		    std::format(".TP\n{}\n{}\n", RoffSyntax(OptionSyntax(option)), RoffText(option.help));
	return text + ".RE\n";
}

// ---- The README ----

// Text as Markdown prints it: the characters that would start a code span,
// emphasis, a link or HTML are written out.
std::string MarkdownText(const std::string& text) {
	std::string safe;
	for (char c : text) {
		if (std::string("\\`*_[]<").find(c) != std::string::npos)
			safe += '\\';
		safe += c;
	}
	return safe;
}

std::string MarkdownCommand(const Command& of) {
	std::string text = std::format("- `catalist {}`: {}.", UsageLine(of), MarkdownText(of.summary));
	if (not of.aliases.empty()) {
		std::vector<std::string> also;
		for (const auto& alias : of.aliases)
			also.push_back(std::format("`{}`", alias));
		text += std::format(" Also {}.", Joined(also, ", "));
	}
	return text + "\n";
}

}  // namespace

std::string Zsh(const std::vector<Command>& table) {
	std::string text =
	    "#compdef catalist\n\n"
	    "# Completion for catalist in zsh. `catalist completion zsh` writes it from\n"
	    "# the program's table of commands: change that, not this file.\n\n";
	text += kZshHelpers;
	text +=
	    "\n_catalist() {\n"
	    "\tlocal curcontext=\"$curcontext\" state state_descr line ret=1\n"
	    "\tlocal -A opt_args\n"
	    "\tlocal _catalist_program=$words[1]\n"
	    "\tlocal -a _catalist_names\n"
	    "\t_catalist_names=(\n";
	for (const Command* each : ShownOf(table))
		text += std::format("\t\t{}\n", ZshQuoted(std::format("{}:{}", each->name, each->summary)));
	text +=
	    "\t)\n\n"
	    "\t_arguments -C :";
	// Help and the version are all that happens: nothing goes with them.
	for (const auto& option : GlobalOptions()) {
		std::vector<std::string> specs = ZshOptionSpecs(option);
		if (option.name == "help" or option.name == "version")
			specs = {std::format("(- *)-{}[{}]", option.letter, ZshExplained(option.help)),
			         std::format("(- *)--{}[{}]", option.name, ZshExplained(option.help))};
		for (const auto& spec : specs)
			text += std::format(" \\\n\t\t{}", ZshQuoted(spec));
	}
	text +=
	    " \\\n"
	    "\t\t'1:command:_catalist_commands' \\\n"
	    "\t\t'*:: :->arguments' && ret=0\n"
	    "\t[[ $state == arguments ]] || return ret\n\n"
	    "\tcase $words[1] in\n";
	for (const Command* each : ShownOf(table))
		text += ZshCommand(*each);
	text +=
	    "\tesac\n"
	    "\treturn ret\n"
	    "}\n\n"
	    "# Loaded from $fpath, the file is the function; read with `source`, it\n"
	    "# has to say which programs it completes.\n"
	    "if [[ $funcstack[1] == _catalist ]]; then\n"
	    "\t_catalist \"$@\"\n"
	    "else\n"
	    "\tcompdef _catalist catalist\n"
	    "fi\n";
	return text;
}

std::string Fish(const std::vector<Command>& table) {
	std::string text =
	    "# Completion for catalist in fish. `catalist completion fish` writes it\n"
	    "# from the program's table of commands: change that, not this file.\n\n";
	text += Replaced(kFishHelpers, "@VALUED@", Joined(GlobalsApart(), " "));
	text +=
	    "\n# No files, but where a command reads them.\n"
	    "complete -c catalist -f\n";
	for (const auto& option : GlobalOptions())
		text +=
		    std::format("complete -c catalist -n __catalist_no_command{}\n", FishOption(option));
	text += FishCommandNames(table, "__catalist_no_command");
	for (const Command* each : ShownOf(table))
		text += "\n" + FishCommand(table, *each);
	return text;
}

std::string Manual(const std::vector<Command>& table) {
	std::string text;
	for (const Command* each : ShownOf(table))
		text += RoffCommand(*each);
	return text;
}

std::string Readme(const std::vector<Command>& table) {
	std::string text;
	for (const Command* each : ShownOf(table))
		text += MarkdownCommand(*each);
	text += "\nOptions, before the command:\n\n";
	for (const auto& option : GlobalOptions())
		text += std::format("- `{}`: {}\n", OptionSyntax(option), MarkdownText(option.help));
	return text;
}

}  // namespace catalist::completion
