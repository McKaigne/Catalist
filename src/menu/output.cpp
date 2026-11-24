#include "menu/output.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/text.hpp"

namespace catalist::output {

namespace {

constexpr int kGap = 2;
constexpr int kNarrowest = 10;

std::vector<int> ColumnWidths(const std::vector<TableColumn>& columns,
                              const std::vector<std::vector<Styled>>& rows, int width) {
	std::vector<int> widths(columns.size(), 0);
	for (const auto& row : rows)
		for (size_t i = 0; i < columns.size() and i < row.size(); ++i)
			widths[i] = std::max(widths[i], utf8::Width(row[i].text));

	int total = kGap * (static_cast<int>(columns.size()) - 1);
	for (int each : widths)
		total += each;
	for (size_t i = 0; i < columns.size(); ++i) {
		if (columns[i].flexible and total > width)
			widths[i] = std::max(kNarrowest, widths[i] - (total - width));
	}
	return widths;
}

std::string Aligned(const std::string& value, int width, const TableColumn& column, bool last) {
	int used = utf8::Width(value);
	if (used > width)
		return utf8::Fit(value, width);
	std::string padding(static_cast<size_t>(width - used), ' ');
	if (column.align_right)
		return padding + value;
	return last ? value : value + padding;
}

std::string TableLine(const std::vector<TableColumn>& columns, const std::vector<int>& widths,
                      const std::vector<Styled>& cells, bool color) {
	size_t count = std::min(columns.size(), cells.size());
	while (count > 0 and cells[count - 1].text.empty())
		--count;
	std::string line;
	for (size_t i = 0; i < count; ++i) {
		bool last = i + 1 == count;
		line += Paint(Aligned(cells[i].text, widths[i], columns[i], last), cells[i].sgr, color);
		line += last ? "" : std::string(kGap, ' ');
	}
	return line + "\n";
}

}  // namespace

std::string DateText(const std::optional<IsoDate>& value) {
	return value ? value->ToString() : "-";
}

std::string Stars(double out_of_ten) {
	int halves = std::clamp(static_cast<int>(std::lround(out_of_ten)), 0, 10);
	if (halves == 0)
		return "-";
	std::string row;
	for (int star = 0; star < 5; ++star) {
		int left = halves - star * 2;
		row += left >= 2 ? "\U000F04CE" : left == 1 ? "\U000F04D0" : "\U000F04D2";
	}
	return row;
}

std::string Paint(const std::string& value, const char* sgr, bool color) {
	return color and sgr ? std::format("\x1b[{}m{}\x1b[0m", sgr, value) : value;
}

std::string Rows(const std::vector<TableColumn>& columns,
                 const std::vector<std::vector<Styled>>& rows, int width, bool color) {
	auto widths = ColumnWidths(columns, rows, width);
	std::string drawn;
	for (const auto& row : rows)
		drawn += TableLine(columns, widths, row, color);
	return drawn;
}

std::string Sections(const std::vector<TableColumn>& columns, const std::vector<Section>& sections,
                     int width, bool color) {
	std::vector<std::vector<Styled>> all;
	for (const auto& section : sections)
		all.insert(all.end(), section.rows.begin(), section.rows.end());
	auto widths = ColumnWidths(columns, all, width);
	std::string drawn;
	for (const auto& section : sections) {
		if (section.rows.empty())
			continue;
		drawn += drawn.empty() ? "" : "\n";
		drawn += Paint(section.heading, kBold, color) + "\n";
		for (const auto& row : section.rows)
			drawn += TableLine(columns, widths, row, color);
	}
	return drawn;
}

}  // namespace catalist::output
