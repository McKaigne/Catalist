#include "output.hpp"

#include <fmt/format.h>

#include <algorithm>

#include "text.hpp"

namespace catalist::output {

namespace {

constexpr int kGap = 2;
// The flexible column is never squeezed below this.
constexpr int kNarrowest = 10;

std::string oneLine(std::string field) {
	std::replace_if(
	    field.begin(), field.end(), [](char c) { return c == '\t' or c == '\n' or c == '\r'; },
	    ' ');
	return field;
}

// The width each column needs, the flexible one cut back to fit `width`.
std::vector<int> columnWidths(const std::vector<tableColumn>& columns,
                              const std::vector<std::vector<styled>>& rows, int width) {
	std::vector<int> widths;
	for (const auto& column : columns)
		widths.push_back(utf8::width(column.header));
	for (const auto& row : rows)
		for (size_t i = 0; i < columns.size() and i < row.size(); ++i)
			widths[i] = std::max(widths[i], utf8::width(row[i].text));

	int total = kGap * (static_cast<int>(columns.size()) - 1);
	for (int each : widths)
		total += each;
	for (size_t i = 0; i < columns.size(); ++i) {
		if (columns[i].flexible and total > width)
			widths[i] = std::max(kNarrowest, widths[i] - (total - width));
	}
	return widths;
}

// The text in exactly `width` columns; the last one of a line is not
// padded on its right.
std::string aligned(const std::string& value, int width, const tableColumn& column, bool last) {
	int used = utf8::width(value);
	if (used > width)
		return utf8::fit(value, width);
	std::string padding(static_cast<size_t>(width - used), ' ');
	if (column.alignRight)
		return padding + value;
	return last ? value : value + padding;
}

// One line of the table. It ends at the last cell that holds anything, so
// no line ends in spaces.
std::string tableLine(const std::vector<tableColumn>& columns, const std::vector<int>& widths,
                      const std::vector<styled>& cells, bool color) {
	size_t count = std::min(columns.size(), cells.size());
	while (count > 0 and cells[count - 1].text.empty())
		--count;
	std::string line;
	for (size_t i = 0; i < count; ++i) {
		bool last = i + 1 == count;
		line += paint(aligned(cells[i].text, widths[i], columns[i], last), cells[i].sgr, color);
		line += last ? "" : std::string(kGap, ' ');
	}
	return line + "\n";
}

}  // namespace

cell text(std::string value) {
	return {std::move(value), jsonForm::kString};
}

cell number(long long value) {
	return {std::to_string(value), jsonForm::kNumber};
}

cell known(int value) {
	return {std::to_string(value), value > 0 ? jsonForm::kNumber : jsonForm::kNull};
}

cell date(const std::optional<isoDate>& value) {
	return value ? cell{value->toString(), jsonForm::kString} : cell{"-", jsonForm::kNull};
}

cell maybe(const std::string& value) {
	return value.empty() ? cell{"-", jsonForm::kNull} : cell{value, jsonForm::kString};
}

std::string tsvLine(const std::vector<std::string>& fields) {
	std::string line;
	for (size_t i = 0; i < fields.size(); ++i)
		line += (i > 0 ? "\t" : "") + oneLine(fields[i]);
	return line + "\n";
}

std::string tsvLine(const std::vector<cell>& cells) {
	std::vector<std::string> fields;
	for (const auto& each : cells)
		fields.push_back(each.text);
	return tsvLine(fields);
}

std::string csvLine(const std::vector<std::string>& fields) {
	std::string line;
	for (size_t i = 0; i < fields.size(); ++i) {
		const std::string& field = fields[i];
		line += i > 0 ? "," : "";
		if (field.find_first_of(",\"\r\n") == std::string::npos) {
			line += field;
			continue;
		}
		line += '"';
		for (char c : field)
			line += c == '"' ? std::string("\"\"") : std::string(1, c);
		line += '"';
	}
	return line + "\n";
}

std::string csvLine(const std::vector<cell>& cells) {
	std::vector<std::string> fields;
	for (const auto& each : cells)
		fields.push_back(each.json == jsonForm::kNull ? "" : each.text);
	return csvLine(fields);
}

std::string jsonString(const std::string& value) {
	std::string quoted = "\"";
	for (unsigned char c : value) {
		switch (c) {
			case '"':
				quoted += "\\\"";
				break;
			case '\\':
				quoted += "\\\\";
				break;
			case '\n':
				quoted += "\\n";
				break;
			case '\r':
				quoted += "\\r";
				break;
			case '\t':
				quoted += "\\t";
				break;
			default:
				// The other control characters have no short form.
				quoted +=
				    c < 0x20 ? fmt::format("\\u{:04x}", c) : std::string(1, static_cast<char>(c));
		}
	}
	return quoted + "\"";
}

std::string jsonLine(const std::vector<std::string>& names, const std::vector<cell>& cells) {
	std::string line = "{";
	for (size_t i = 0; i < names.size() and i < cells.size(); ++i) {
		const cell& each = cells[i];
		std::string value = each.json == jsonForm::kNull     ? "null"
		                    : each.json == jsonForm::kNumber ? each.text
		                                                     : jsonString(each.text);
		line += fmt::format("{}{}:{}", i > 0 ? "," : "", jsonString(names[i]), value);
	}
	return line + "}\n";
}

std::string paint(const std::string& value, const char* sgr, bool color) {
	return color and sgr ? fmt::format("\x1b[{}m{}\x1b[0m", sgr, value) : value;
}

std::string table(const std::vector<tableColumn>& columns,
                  const std::vector<std::vector<styled>>& rows, int width, bool color) {
	auto widths = columnWidths(columns, rows, width);
	std::vector<styled> header;
	for (const auto& column : columns)
		header.push_back({column.header, kDim});
	std::string drawn = tableLine(columns, widths, header, color);
	for (const auto& row : rows)
		drawn += tableLine(columns, widths, row, color);
	return drawn;
}

}  // namespace catalist::output
