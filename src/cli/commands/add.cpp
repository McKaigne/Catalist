#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"

namespace catalist::cli {

int runAdd(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  const iClock& clock = ctx.clock;
  if (args.empty()) {
    emitError("usage", "usage: catalist add <title> [flags]", false);
    return 2;
  }

  std::string title;
  mediaType type = mediaType::kAnime;
  watchStatus status = watchStatus::kWatching;
  int score = 0;
  int progress = 0;
  int totalUnits = 0;
  int volumes = 0;
  std::optional<isoDate> startDate;
  std::optional<isoDate> finishDate;
  std::string notes;
  bool jsonMode = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--json") {
      jsonMode = true;
    } else if (arg == "--type" || arg == "-t") {
      auto value = flagValue(args, i, "missing argument for '--type'", jsonMode);
      if (!value)
        return 2;
      auto parsedType = parseMediaTypeArgument(*value);
      if (!parsedType) {
        emitError(
            "usage",
            "invalid type '" + *value + "'. Expected 'anime' or 'manga'.",
            jsonMode);
        return 2;
      }
      type = *parsedType;
    } else if (arg == "--status" || arg == "-s") {
      auto value = flagValue(args, i, "missing argument for '--status'", jsonMode);
      if (!value)
        return 2;
      auto parsedStatus = parseStatus(*value);
      if (!parsedStatus) {
        emitError("usage", "invalid status '" + *value + "'. " + kStatusChoices,
                  jsonMode);
        return 2;
      }
      status = *parsedStatus;
    } else if (arg == "--rating" || arg == "-r" || arg == "--score") {
      auto value = flagValue(args, i, "missing argument for '--rating'", jsonMode);
      auto parsed = value ? boundedInteger(*value, 0, 10,
                                           "invalid rating '" + *value +
                                               "'. Expected 0..10.",
                                           jsonMode)
                          : std::nullopt;
      if (!parsed)
        return 2;
      score = *parsed;
    } else if (arg == "--progress" || arg == "-p") {
      auto parsed = countFlag(args, i, "--progress", jsonMode);
      if (!parsed)
        return 2;
      progress = *parsed;
    } else if (arg == "--episodes" || arg == "--chapters" || arg == "--total" ||
               arg == "-e") {
      auto parsed = countFlag(args, i, "--total", jsonMode);
      if (!parsed)
        return 2;
      totalUnits = *parsed;
    } else if (arg == "--volumes" || arg == "-v") {
      auto parsed = countFlag(args, i, "--volumes", jsonMode);
      if (!parsed)
        return 2;
      volumes = *parsed;
    } else if (arg == "--start" || arg == "--started") {
      auto value = flagValue(args, i, "missing date for '--start'", jsonMode);
      auto parsedDate = value ? parseDateArgument(*value, "--start", jsonMode)
                              : std::nullopt;
      if (!parsedDate)
        return 2;
      startDate = *parsedDate;
    } else if (arg == "--finish" || arg == "--finished") {
      auto value = flagValue(args, i, "missing date for '--finish'", jsonMode);
      auto parsedDate = value ? parseDateArgument(*value, "--finish", jsonMode)
                              : std::nullopt;
      if (!parsedDate)
        return 2;
      finishDate = *parsedDate;
    } else if (arg == "--notes" || arg == "-n") {
      auto value = flagValue(args, i, "missing text for '--notes'", jsonMode);
      if (!value)
        return 2;
      notes = *value;
    } else if (title.empty() && arg.rfind("-", 0) != 0) {
      title = arg;
    } else {
      emitError("usage", "unknown option or unexpected argument: '" + arg + "'",
                jsonMode);
      return 2;
    }
  }

  if (title.empty()) {
    emitError("usage", "missing title for 'catalist add <title> [flags]'.",
              jsonMode);
    return 2;
  }

  if (!startDate)
    startDate = localDate(clock);

  entry e;
  e.type = type;
  e.title = title;
  e.status = status;
  e.score = score;
  e.progress = progress;
  e.totalUnits = totalUnits;
  e.volumes = volumes;
  e.startDate = startDate;
  e.finishDate = finishDate;
  e.notes = notes;
  e.updatedAt = startDate;

  int assignedId = db.addEntry(e);
  db.saveAll();

  if (jsonMode) {
    auto opt = db.getById(type, assignedId);
    if (opt.has_value()) {
      std::cout << formatJsonEntry(opt.value()) << "\n";
    }
  } else {
    bool color = shouldColor(ctx.interactive);
    std::cout << (color ? ansi::kGreen + ansi::kBold : "") << "Added "
              << (type == mediaType::kAnime ? "anime" : "manga") << " '" << title
              << "' [ID " << assignedId << "]" << (color ? ansi::kReset : "")
              << "\n";
  }

  return 0;
}

}  // namespace catalist::cli
