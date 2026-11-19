#include "cli/args.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <istream>
#include <numeric>

namespace catalist::args {

namespace {

std::string KeyOf(const Option& taken) {
	return taken.name.empty() ? std::string(1, taken.letter) : taken.name;
}

const Option* FindLong(const std::vector<Option>& options, const std::string& name) {
	auto found = std::find_if(options.begin(), options.end(), [&name](const Option& each) {
		return not each.name.empty() and each.name == name;
	});
	return found == options.end() ? nullptr : &*found;
}

const Option* FindShort(const std::vector<Option>& options, char letter) {
	auto found = std::find_if(options.begin(), options.end(),
	                          [letter](const Option& each) { return each.letter == letter; });
	return found == options.end() ? nullptr : &*found;
}

std::string UnknownLong(const std::vector<Option>& options, const std::string& name) {
	std::vector<std::string> names;
	for (const auto& each : options)
		if (not each.name.empty())
			names.push_back(each.name);
	auto guess = ClosestTo(name, names);
	return std::format("unknown option '--{}'{}", name,
	                   guess ? std::format(" (did you mean '--{}'?)", *guess) : "");
}

// Reads the arguments one at a time; `next` is the one being read.
struct Reader {
	const std::vector<std::string>& arguments;
	const std::vector<Option>& options;
	size_t next = 0;
	Parsed line{};

	// The value of an option that has none attached: the next argument.
	[[nodiscard]] std::optional<std::string> TakeValue() {
		if (next >= arguments.size())
			return std::nullopt;
		return arguments[next++];
	}

	// --name, --name=value or --name value. Returns an error message, or "".
	[[nodiscard]] std::string ReadLong(const std::string& argument) {
		auto equals = argument.find('=');
		std::string name = argument.substr(2, equals == std::string::npos ? equals : equals - 2);
		const Option* found = FindLong(options, name);
		if (not found)
			return UnknownLong(options, name);
		if (found->value.empty()) {
			if (equals != std::string::npos)
				return std::format("option '--{}' takes no value", name);
			line.options[KeyOf(*found)] = "";
			return "";
		}
		auto value = equals != std::string::npos ? argument.substr(equals + 1) : TakeValue();
		if (not value)
			return std::format("option '--{}' needs a value ({})", name, found->value);
		line.options[KeyOf(*found)] = *value;
		return "";
	}

	// -a, -am, -n 3, -n3 or -an3. Returns an error message, or "".
	[[nodiscard]] std::string ReadShort(const std::string& argument) {
		for (size_t at = 1; at < argument.size(); ++at) {
			const Option* found = FindShort(options, argument[at]);
			if (not found)
				return std::format("unknown option '-{}'", argument[at]);
			if (found->value.empty()) {
				line.options[KeyOf(*found)] = "";
				continue;
			}
			// What is left of the argument is the value; else the next one.
			auto value = at + 1 < argument.size() ? argument.substr(at + 1) : TakeValue();
			if (not value)
				return std::format("option '-{}' needs a value ({})", argument[at], found->value);
			line.options[KeyOf(*found)] = *value;
			return "";
		}
		return "";
	}
};

}  // namespace

std::string Spelled(const Option& taken) {
	return taken.name.empty() ? std::format("-{}", taken.letter) : std::format("--{}", taken.name);
}

size_t EditDistance(const std::string& from, const std::string& to) {
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

std::string Parsed::Get(const std::string& name) const {
	auto found = options.find(name);
	return found == options.end() ? "" : found->second;
}

Result<Parsed> Parse(const std::vector<std::string>& arguments,
                     const std::vector<Option>& options) {
	Reader read{arguments, options};
	while (read.next < arguments.size()) {
		const std::string& argument = arguments[read.next];
		// The first operand ends the options; a lone - is an operand.
		if (argument.size() < 2 or argument[0] != '-')
			break;
		++read.next;
		if (argument == "--")
			break;
		std::string error = argument[1] == '-' ? read.ReadLong(argument) : read.ReadShort(argument);
		if (not error.empty())
			return std::unexpected(error);
	}
	read.line.operands.assign(arguments.begin() + static_cast<std::ptrdiff_t>(read.next),
	                          arguments.end());
	return std::move(read.line);
}

std::vector<std::string> WithLinesFor(const std::vector<std::string>& operands, std::istream& in) {
	std::vector<std::string> all;
	for (const auto& operand : operands) {
		if (operand != "-") {
			all.push_back(operand);
			continue;
		}
		std::string line;
		while (std::getline(in, line)) {
			if (not line.empty() and line.back() == '\r')
				line.pop_back();
			if (line.find_first_not_of(" \t") != std::string::npos)
				all.push_back(line);
		}
	}
	return all;
}

std::optional<std::string> ClosestTo(const std::string& typed,
                                     const std::vector<std::string>& known) {
	std::optional<std::string> best;
	size_t best_distance = 3;  // further than two edits is not a typo
	for (const auto& word : known) {
		size_t distance = EditDistance(typed, word);
		if (distance < best_distance and distance < typed.size()) {
			best = word;
			best_distance = distance;
		}
	}
	return best;
}

std::optional<int> ParseCount(const std::string& text) {
	if (text.empty() or text.size() > 6 or
	    not std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); }))
		return std::nullopt;
	return std::stoi(text);
}

std::optional<int> ParseId(const std::string& text) {
	if (text.empty() or
	    not std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); }))
		return std::nullopt;
	int id = 0;
	auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
	if (error != std::errc() or end != text.data() + text.size() or id <= 0)
		return std::nullopt;
	return id;
}

}  // namespace catalist::args
