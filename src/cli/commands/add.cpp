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
      if (++i >= args.size()) {
        emitError("usage", "missing argument for '--type'", jsonMode);
        return 2;
      }
      auto parsedType = parseMediaTypeArgument(args[i]);
      if (!parsedType) {
        emitError(
            "usage",
            "invalid type '" + args[i] + "'. Expected 'anime' or 'manga'.",
            jsonMode);
        return 2;
      }
      type = *parsedType;
    } else if (arg == "--status" || arg == "-s") {
      if (++i >= args.size()) {
        emitError("usage", "missing argument for '--status'", jsonMode);
        return 2;
      }
      auto parsedStatus = parseStatus(args[i]);
      if (!parsedStatus) {
        emitError("usage", "invalid status '" + args[i] + "'. " + kStatusChoices,
                  jsonMode);
        return 2;
      }
      status = *parsedStatus;
    } else if (arg == "--rating" || arg == "-r" || arg == "--score") {
      if (++i >= args.size()) {
        emitError("usage", "missing argument for '--rating'", jsonMode);
        return 2;
      }
      try {
        score = std::clamp(std::stoi(args[i]), 0, 10);
      } catch (...) {
        emitError("usage", "invalid rating '" + args[i] + "'. Expected 0..10.",
                  jsonMode);
        return 2;
      }
    } else if (arg == "--progress" || arg == "-p") {
      if (++i >= args.size()) {
        emitError("usage", "missing argument for '--progress'", jsonMode);
        return 2;
      }
      try {
        progress = std::max(0, std::stoi(args[i]));
      } catch (...) {
        emitError("usage", "invalid integer for '--progress': " + args[i],
                  jsonMode);
        return 2;
      }
    } else if (arg == "--episodes" || arg == "--chapters" || arg == "--total" ||
               arg == "-e") {
      if (++i >= args.size()) {
        emitError("usage", "missing argument for '--total'", jsonMode);
        return 2;
      }
      try {
        totalUnits = std::max(0, std::stoi(args[i]));
      } catch (...) {
        emitError("usage", "invalid integer for '--total': " + args[i],
                  jsonMode);
        return 2;
      }
    } else if (arg == "--volumes" || arg == "-v") {
      if (++i >= args.size()) {
        emitError("usage", "missing argument for '--volumes'", jsonMode);
        return 2;
      }
      try {
        volumes = std::max(0, std::stoi(args[i]));
      } catch (...) {
        emitError("usage", "invalid integer for '--volumes': " + args[i],
                  jsonMode);
        return 2;
      }
    } else if (arg == "--start" || arg == "--started") {
      if (++i >= args.size()) {
        emitError("usage", "missing date for '--start'", jsonMode);
        return 2;
      }
      auto parsedDate = parseDateArgument(args[i], "--start", jsonMode);
      if (!parsedDate)
        return 2;
      startDate = *parsedDate;
    } else if (arg == "--finish" || arg == "--finished") {
      if (++i >= args.size()) {
        emitError("usage", "missing date for '--finish'", jsonMode);
        return 2;
      }
      auto parsedDate = parseDateArgument(args[i], "--finish", jsonMode);
      if (!parsedDate)
        return 2;
      finishDate = *parsedDate;
    } else if (arg == "--notes" || arg == "-n") {
      if (++i >= args.size()) {
        emitError("usage", "missing text for '--notes'", jsonMode);
        return 2;
      }
      notes = args[i];
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
  e.dirty = true;

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
