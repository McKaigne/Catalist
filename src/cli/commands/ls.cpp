#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/storage/csvCodec.hpp"

namespace catalist::cli {

int runLs(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  const appSettings& settings = ctx.settings;
  cliFilterOptions defaults;
  defaults.reverseSort = settings.reverseSort;
  auto parsed = parseCommonFilters(args, defaults);
  if (!parsed.ok()) {
    emitError("usage", parsed.errorValue().message,
              parsed.errorValue().jsonErrors);
    return 2;
  }
  const cliFilterOptions& opts = parsed.value();

  auto entries = collectAndFilterEntries(db, opts);

  if (opts.format == outputFormat::kJson) {
    std::cout << formatJsonEntries(entries);
    return 0;
  }

  if (opts.format == outputFormat::kPlain) {
    for (const auto& e : entries) {
      std::cout << e.title << "\n";
    }
    return 0;
  }

  if (opts.format == outputFormat::kIds) {
    for (const auto& e : entries) {
      std::cout << e.id << "\n";
    }
    return 0;
  }

  if (opts.format == outputFormat::kCsv) {
    std::cout
        << "id,mal_id,type,title,status,score,progress,total_units,start_date,finish_date\n";
    for (const auto& e : entries) {
      std::cout << e.id << "," << e.malId << ","
                << (e.type == mediaType::kAnime ? "anime" : "manga") << ","
                << csvCodec::escapeField(e.title) << ","
                << csvCodec::escapeField(statusToString(e.status, e.type)) << "," << e.score
                << "," << e.progress << "," << e.totalUnits << ","
                << csvCodec::escapeField(formatDate(e.startDate)) << "," << csvCodec::escapeField(formatDate(e.finishDate))
                << "\n";
    }
    return 0;
  }

  bool tty = ctx.interactive;
  bool color = shouldColor(ctx.interactive);

  int maxIdLen = 4;
  for (const auto& e : entries) {
    maxIdLen =
        std::max(maxIdLen, static_cast<int>(std::to_string(e.id).size()));
  }

  std::ostringstream out;
  if (tty && color)
    out << ansi::kBold << ansi::kMauve;
  out << std::right << std::setw(maxIdLen + 2) << "ID" << "  " << std::left
      << std::setw(30) << "TITLE" << "  " << std::setw(7) << "TYPE" << "  "
      << std::setw(12) << "STATUS" << "  " << std::right << std::setw(7)
      << "RATING" << "  " << std::setw(9) << "PROGRESS";
  if (tty && color)
    out << ansi::kReset;
  out << "\n";

  if (tty) {
    if (color)
      out << ansi::kDim << ansi::kSubtext;
    out << std::string(maxIdLen + 2, '-') << "  " << std::string(30, '-')
        << "  " << std::string(7, '-') << "  " << std::string(12, '-') << "  "
        << std::string(7, '-') << "  " << std::string(9, '-');
    if (color)
      out << ansi::kReset;
    out << "\n";
  }

  for (const auto& e : entries) {
    std::string titleDisplay = e.title;
    if (titleDisplay.size() > 28) {
      titleDisplay = titleDisplay.substr(0, 25) + "...";
    }

    std::string scoreStr =
        (e.score > 0) ? (std::to_string(e.score) + "/10") : "-";
    std::string progStr =
        std::to_string(e.progress) + "/" +
        (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?");

    out << std::right << std::setw(maxIdLen + 2) << e.id << "  " << std::left
        << std::setw(30) << titleDisplay << "  " << std::setw(7)
        << (e.type == mediaType::kAnime ? "anime" : "manga") << "  "
        << std::setw(12) << statusToString(e.status, e.type) << "  "
        << std::right << std::setw(7) << scoreStr << "  " << std::setw(9)
        << progStr << "\n";
  }

  if (tty) {
    out << "\n";
    if (color)
      out << ansi::kSubtext;
    out << entries.size() << " entries shown\n";
    if (color)
      out << ansi::kReset;
  }

  std::cout << out.str();
  return 0;
}

}  // namespace catalist::cli
