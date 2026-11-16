#include "cli/completion.hpp"

#include <algorithm>
#include <cctype>
#include <format>

#include "core/text.hpp"

namespace catalist::completion {

namespace {

std::string joined(const std::vector<std::string>& words, const std::string& between) {
	std::string all;
	for (const auto& word : words)
		all += (all.empty() ? "" : between) + word;
	return all;
}

// Every `from` in the text written as `to`.
std::string replaced(std::string text, const std::string& from, const std::string& to) {
	for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		text.replace(at, from.size(), to);
	return text;
}

// The command's name and the other names it goes by.
std::vector<std::string> namesOf(const command& of) {
	std::vector<std::string> names = {of.name};
	names.insert(names.end(), of.aliases.begin(), of.aliases.end());
	return names;
}

// The program's own options that take a value, as written with the value
// apart ("--color always"): the scripts skip that value with them.
std::vector<std::string> globalsApart() {
	std::vector<std::string> spellings;
	for (const auto& each : globalOptions()) {
		if (each.value.empty())
			continue;
		if (each.letter)
			spellings.push_back(std::format("-{}", each.letter));
		if (not each.name.empty())
			spellings.push_back("--" + each.name);
	}
	return spellings;
}

std::vector<const command*> shownOf(const std::vector<command>& table) {
	std::vector<const command*> shown;
	for (const auto& each : table)
		if (not each.hidden)
			shown.push_back(&each);
	return shown;
}

// "REF...", "[QUERY]" and "FIELD=VALUE" as the word for what is asked for:
// "ref", "query", "field". Words to choose from, as "zsh|fish", are a choice.
std::string wordFor(const std::string& operands) {
	if (operands.find('|') != std::string::npos)
		return "choice";
	std::string word;
	for (char c : operands) {
		if (not std::isalpha(static_cast<unsigned char>(c)) and not word.empty())
			break;
		if (std::isalpha(static_cast<unsigned char>(c)))
			word += c;
	}
	return utf8::lowercase(word);
}

// ---- zsh ----

std::string zshQuoted(const std::string& text) {
	return std::format("'{}'", replaced(text, "'", "'\\''"));
}

// The words in the brackets of an option: a bracket in them would end them.
std::string zshExplained(const std::string& help) {
	std::string safe = replaced(help, "\\", "\\\\");
	safe = replaced(safe, "[", "\\[");
	return replaced(safe, "]", "\\]");
}

// What follows an option's name: its explanation and, when it takes a
// value, what the value is and the values to choose from.
std::string zshOptionTail(const args::option& taken) {
	std::string tail = std::format("[{}]", zshExplained(taken.help));
	if (taken.value.empty())
		return tail;
	std::string choices =
	    taken.choices.empty() ? " " : std::format("({})", joined(taken.choices, " "));
	return tail + std::format(":{}:{}", utf8::lowercase(taken.value), choices);
}

// One spec of _arguments for each way the option is written. The two ways
// of one option rule each other out.
std::vector<std::string> zshOptionSpecs(const args::option& taken) {
	std::string tail = zshOptionTail(taken);
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

std::vector<std::string> zshOperandSpecs(const command& of) {
	std::string word = wordFor(of.operands);
	switch (of.completes) {
		case operandKind::kNone:
			return {};
		case operandKind::kRefs:
			return {"*:title:_catalist_named"};
		case operandKind::kRefThenFields:
			return {"1:title:_catalist_named", "*:field:_catalist_fields"};
		case operandKind::kTitle:
			return {"1:title:_catalist_titles"};
		case operandKind::kText:
			return {std::format("1:{}: ", word)};
		case operandKind::kFiles:
			return {"*:file:_files"};
		case operandKind::kWords:
			return {std::format("1:{}:({})", word, joined(of.words, " "))};
		case operandKind::kCommands:
			return {"1:command:_catalist_commands"};
	}
	return {};
}

// "ls)" or "'+'|inc)": the case that takes the command by any of its names.
std::string zshCase(const command& of) {
	std::vector<std::string> names;
	for (const auto& name : namesOf(of))
		names.push_back(std::isalpha(static_cast<unsigned char>(name[0])) ? name : zshQuoted(name));
	return joined(names, "|") + ")";
}

std::string zshCommand(const command& of) {
	std::vector<std::string> specs;
	std::vector<args::option> options = of.options;
	options.push_back(helpOption());
	for (const auto& option : options)
		for (const auto& spec : zshOptionSpecs(option))
			specs.push_back(spec);
	for (const auto& spec : zshOperandSpecs(of))
		specs.push_back(spec);

	std::string text = std::format("\t{}\n\t\t_arguments -s -S -A '-*' :", zshCase(of));
	for (const auto& spec : specs)
		text += std::format(" \\\n\t\t\t{}", zshQuoted(spec));
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

std::string fishQuoted(const std::string& text) {
	return std::format("'{}'", replaced(replaced(text, "\\", "\\\\"), "'", "\\'"));
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

std::string fishOption(const args::option& taken) {
	std::string text;
	if (taken.letter)
		text += std::format(" -s {}", taken.letter);
	if (not taken.name.empty())
		text += std::format(" -l {}", taken.name);
	if (not taken.value.empty())
		text += " -x";
	if (not taken.choices.empty())
		text += std::format(" -a {}", fishQuoted(joined(taken.choices, " ")));
	return text + std::format(" -d {}", fishQuoted(taken.help));
}

// What `complete` is given for the command's operands, each with the
// condition (beyond the command itself) under which it holds.
std::vector<std::pair<std::string, std::string>> fishOperands(const command& of) {
	const std::string first = "; and test (__catalist_operands) -eq 0";
	const std::string later = "; and test (__catalist_operands) -ge 1";
	switch (of.completes) {
		case operandKind::kNone:
		case operandKind::kText:
		case operandKind::kCommands:
			return {};
		case operandKind::kRefs:
			return {{"", "-k -a '(__catalist_named)'"}};
		case operandKind::kRefThenFields:
			return {{first, "-k -a '(__catalist_named)'"}, {later, "-a '(__catalist_fields)'"}};
		case operandKind::kTitle:
			return {{first, "-a '(__catalist_ask titles)'"}};
		case operandKind::kFiles:
			return {{"", "-F"}};
		case operandKind::kWords:
			return {{first, std::format("-a {}", fishQuoted(joined(of.words, " ")))}};
	}
	return {};
}

// The lines that offer the commands themselves, under `condition`.
std::string fishCommandNames(const std::vector<command>& table, const std::string& condition) {
	std::string text;
	for (const command* each : shownOf(table))
		text += std::format("complete -c catalist -n {} -a {} -d {}\n", fishQuoted(condition),
		                    fishQuoted(each->name), fishQuoted(each->summary));
	return text;
}

std::string fishCommand(const std::vector<command>& table, const command& of) {
	std::string condition = std::format("__catalist_command {}", joined(namesOf(of), " "));
	std::string text;
	std::vector<args::option> options = of.options;
	options.push_back(helpOption());
	for (const auto& option : options)
		text += std::format("complete -c catalist -n {}{}\n", fishQuoted(condition),
		                    fishOption(option));
	for (const auto& [when, what] : fishOperands(of))
		text += std::format("complete -c catalist -n {} {}\n", fishQuoted(condition + when), what);
	if (of.completes == operandKind::kCommands)
		text += fishCommandNames(table, condition + "; and test (__catalist_operands) -eq 0");
	return text;
}

// ---- The manual page ----

// Text as roff prints it: a backslash and a hyphen-minus are written out,
// and a line cannot begin with roff's control characters.
std::string roffText(const std::string& text) {
	std::string safe = replaced(replaced(text, "\\", "\\e"), "-", "\\-");
	if (not safe.empty() and (safe[0] == '.' or safe[0] == '\''))
		safe = "\\&" + safe;
	return safe;
}

bool isSyntax(char c) {
	return std::string(" []|=.,").find(c) != std::string::npos;
}

// A usage line or an option in the manual's type: words in capitals stand
// for something and are italic, the rest is typed as written and is bold.
std::string roffSyntax(const std::string& line) {
	std::string styled;
	for (size_t at = 0; at < line.size();) {
		if (isSyntax(line[at])) {
			styled += line[at++];
			continue;
		}
		size_t end = at;
		while (end < line.size() and not isSyntax(line[end]))
			++end;
		std::string word = line.substr(at, end - at);
		bool standsFor = std::none_of(word.begin(), word.end(), [](unsigned char c) {
			return std::islower(c) or c == '-' or c == '+';
		});
		styled += std::format("\\f{}{}\\fR", standsFor ? 'I' : 'B', roffText(word));
		at = end;
	}
	return styled;
}

// "-s STATUS", "--sort=KEY", "-h, --help".
std::string optionSyntax(const args::option& taken) {
	std::string label = taken.letter ? std::format("-{}", taken.letter) : "";
	if (not taken.name.empty())
		label += std::format("{}--{}", label.empty() ? "" : ", ", taken.name);
	if (not taken.value.empty())
		label += std::format("{}{}", taken.name.empty() ? " " : "=", taken.value);
	return label;
}

// A command's `details` under its entry: paragraphs, with the indented
// lines (examples, column names) set as they are written.
std::string roffDetails(const std::string& details) {
	std::string text;
	bool asWritten = false;
	bool newParagraph = true;
	size_t at = 0;
	while (at < details.size()) {
		size_t end = details.find('\n', at);
		std::string line = details.substr(at, end == std::string::npos ? end : end - at);
		at = end == std::string::npos ? details.size() : end + 1;
		if (line.empty()) {
			newParagraph = true;
			continue;
		}
		bool indented = line[0] == ' ';
		if (asWritten and not indented)
			text += ".fi\n";
		if (newParagraph)
			text += ".IP\n";
		if (indented and not asWritten)
			text += ".nf\n";
		asWritten = indented;
		newParagraph = false;
		text += roffText(indented ? line.substr(line.find_first_not_of(' ')) : line) + "\n";
	}
	return text + (asWritten ? ".fi\n" : "");
}

std::string roffCommand(const command& of) {
	std::string text =
	    std::format(".TP\n{}\n{}.\n", roffSyntax(usageLine(of)), roffText(of.summary));
	if (not of.aliases.empty()) {
		std::vector<std::string> also;
		for (const auto& alias : of.aliases)
			also.push_back(std::format("\\fB{}\\fR", roffText(alias)));
		text += std::format("Also {}.\n", joined(also, ", "));
	}
	text += roffDetails(of.details);
	if (of.options.empty())
		return text;
	text += ".RS\n";
	for (const auto& option : of.options)
		text +=
		    std::format(".TP\n{}\n{}\n", roffSyntax(optionSyntax(option)), roffText(option.help));
	return text + ".RE\n";
}

}  // namespace

std::string zsh(const std::vector<command>& table) {
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
	for (const command* each : shownOf(table))
		text += std::format("\t\t{}\n", zshQuoted(std::format("{}:{}", each->name, each->summary)));
	text +=
	    "\t)\n\n"
	    "\t_arguments -C :";
	// Help and the version are all that happens: nothing goes with them.
	for (const auto& option : globalOptions()) {
		std::vector<std::string> specs = zshOptionSpecs(option);
		if (option.name == "help" or option.name == "version")
			specs = {std::format("(- *)-{}[{}]", option.letter, zshExplained(option.help)),
			         std::format("(- *)--{}[{}]", option.name, zshExplained(option.help))};
		for (const auto& spec : specs)
			text += std::format(" \\\n\t\t{}", zshQuoted(spec));
	}
	text +=
	    " \\\n"
	    "\t\t'1:command:_catalist_commands' \\\n"
	    "\t\t'*:: :->arguments' && ret=0\n"
	    "\t[[ $state == arguments ]] || return ret\n\n"
	    "\tcase $words[1] in\n";
	for (const command* each : shownOf(table))
		text += zshCommand(*each);
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

std::string fish(const std::vector<command>& table) {
	std::string text =
	    "# Completion for catalist in fish. `catalist completion fish` writes it\n"
	    "# from the program's table of commands: change that, not this file.\n\n";
	text += replaced(kFishHelpers, "@VALUED@", joined(globalsApart(), " "));
	text +=
	    "\n# No files, but where a command reads them.\n"
	    "complete -c catalist -f\n";
	for (const auto& option : globalOptions())
		text +=
		    std::format("complete -c catalist -n __catalist_no_command{}\n", fishOption(option));
	text += fishCommandNames(table, "__catalist_no_command");
	for (const command* each : shownOf(table))
		text += "\n" + fishCommand(table, *each);
	return text;
}

std::string manual(const std::vector<command>& table) {
	std::string text;
	for (const command* each : shownOf(table))
		text += roffCommand(*each);
	return text;
}

}  // namespace catalist::completion
