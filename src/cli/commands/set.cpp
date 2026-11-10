#include "catalist/cli/commands/commands.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "catalist/cli/args.hpp"
#include "catalist/cli/output.hpp"
#include "catalist/model/entryOps.hpp"

namespace catalist::cli {

namespace {

struct progressChange {
  std::optional<int> absolute;
  int delta = 0;
};

bool isCount(const std::string& text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), ::isdigit);
}

// "add"/"a" and "sub"/"s"/"dec" step by one, or by the count that follows
// them ("-p add 3"), which is then consumed.
int stepCount(const commandArgs& args, size_t& index) {
  if (index + 1 >= args.size() || !isCount(args[index + 1]))
    return 1;
  return std::stoi(args[++index]);
}

// Parses the value of --progress at args[index]: an absolute count, a signed
// delta (+N/-N) or a step word. Prints the usage error and returns nullopt
// on a malformed value.
std::optional<progressChange> parseProgressArgument(const commandArgs& args,
                                                    size_t& index,
                                                    bool jsonMode) {
  const std::string& value = args[index];
  if (value == "add" || value == "a")
    return progressChange{std::nullopt, stepCount(args, index)};
  if (value == "sub" || value == "s" || value == "dec")
    return progressChange{std::nullopt, -stepCount(args, index)};

  bool isSigned = !value.empty() && (value.front() == '+' || value.front() == '-');
  try {
    if (isSigned)
      return progressChange{std::nullopt, std::stoi(value)};
    return progressChange{std::max(0, std::stoi(value)), 0};
  } catch (...) {
    emitError("usage",
              std::string(isSigned ? "invalid delta" : "invalid integer") +
                  " for '--progress': " + value,
              jsonMode);
    return std::nullopt;
  }
}

}  // namespace

int runSet(const commandArgs& args, commandContext& ctx) {
  database& db = ctx.db;
  const iClock& clock = ctx.clock;
  if (args.empty()) {
    emitError("usage", "usage: catalist set <id|title|-> [flags]", false);
    return 2;
  }

  std::string target;
  bool jsonMode = false;
  std::optional<int> absoluteProgress;
  int deltaProgress = 0;
  std::optional<int> setScore;
  std::optional<watchStatus> setStatus;
  std::optional<int> setTotal;
  std::optional<int> setVolumes;
  std::optional<std::optional<isoDate>> setStart;
  std::optional<std::optional<isoDate>> setFinish;
  std::optional<std::string> setNotes;
  std::optional<std::string> setTitle;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--json") {
      jsonMode = true;
    } else if (arg == "--progress" || arg == "-p") {
      auto value = flagValue(args, i, "missing value for '--progress'", jsonMode);
      auto change = value ? parseProgressArgument(args, i, jsonMode)
                          : std::nullopt;
      if (!change)
        return 2;
      if (change->absolute)
        absoluteProgress = change->absolute;
      else
        deltaProgress = change->delta;
    } else if (arg == "--rating" || arg == "-r" || arg == "--score") {
      auto value = flagValue(args, i, "missing argument for '--rating'", jsonMode);
      setScore = value ? boundedInteger(*value, 0, 10,
                                        "invalid score for '--rating': " + *value,
                                        jsonMode)
                       : std::nullopt;
      if (!setScore)
        return 2;
    } else if (arg == "--status" || arg == "-s") {
      auto value = flagValue(args, i, "missing argument for '--status'", jsonMode);
      if (!value)
        return 2;
      setStatus = parseStatus(*value);
      if (!setStatus) {
        emitError("usage", "invalid status '" + *value + "'. " + kStatusChoices,
                  jsonMode);
        return 2;
      }
    } else if (arg == "--episodes" || arg == "--chapters" || arg == "--total" ||
               arg == "-e") {
      setTotal = countFlag(args, i, "--total", jsonMode);
      if (!setTotal)
        return 2;
    } else if (arg == "--volumes" || arg == "-v") {
      setVolumes = countFlag(args, i, "--volumes", jsonMode);
      if (!setVolumes)
        return 2;
    } else if (arg == "--start" || arg == "--started") {
      auto value = flagValue(args, i, "missing date for '--start'", jsonMode);
      setStart = value ? parseDateArgument(*value, "--start", jsonMode)
                       : std::nullopt;
      if (!setStart)
        return 2;
    } else if (arg == "--finish" || arg == "--finished") {
      auto value = flagValue(args, i, "missing date for '--finish'", jsonMode);
      setFinish = value ? parseDateArgument(*value, "--finish", jsonMode)
                        : std::nullopt;
      if (!setFinish)
        return 2;
    } else if (arg == "--notes" || arg == "-n") {
      setNotes = flagValue(args, i, "missing text for '--notes'", jsonMode);
      if (!setNotes)
        return 2;
    } else if (arg == "--title") {
      setTitle = flagValue(args, i, "missing text for '--title'", jsonMode);
      if (!setTitle)
        return 2;
    } else if (target.empty() && arg.rfind("-", 0) != 0) {
      target = arg;
    } else if (target.empty() && arg == "-") {
      target = "-";
    } else {
      emitError("usage", "unknown option or unexpected argument: '" + arg + "'",
                jsonMode);
      return 2;
    }
  }

  if (target.empty()) {
    emitError(
        "usage",
        "missing entry ID or title for 'catalist set <id|title|-> [flags]'.",
        jsonMode);
    return 2;
  }

  std::vector<std::string> queries = collectQueries(target);

  if (queries.empty()) {
    emitError("input", "no target identifiers received from stdin.", jsonMode);
    return 1;
  }

  int successCount = 0;
  for (const auto& q : queries) {
    auto resolved = resolveEntry(db, q);
    if (!resolved.ok()) {
      reportResolveError(resolved.errorValue(), jsonMode, false);
      continue;
    }
    const entry& match = resolved.value();

    isoDate today = localDate(clock);
    // The completion rule runs even without a progress flag, so an entry
    // already at its total is marked completed by any edit.
    auto e = setProgress(match,
                         absoluteProgress.value_or(match.progress + deltaProgress),
                         today);

    if (setScore.has_value())
      e.score = setScore.value();
    if (setStatus.has_value())
      e.status = setStatus.value();
    if (setTotal.has_value())
      e.totalUnits = setTotal.value();
    if (setVolumes.has_value())
      e.volumes = setVolumes.value();
    if (setStart.has_value())
      e.startDate = setStart.value();
    if (setFinish.has_value())
      e.finishDate = setFinish.value();
    if (setNotes.has_value())
      e.notes = setNotes.value();
    if (setTitle.has_value())
      e.title = setTitle.value();

    e.updatedAt = today;
    e.dirty = true;

    db.updateEntry(e);
    successCount++;

    if (jsonMode) {
      std::cout << formatJsonEntry(e) << "\n";
    } else {
      bool color = shouldColor(ctx.interactive);
      std::cout << (color ? ansi::kGreen : "") << "Updated [" << e.id << "] '"
                << e.title << "': "
                << "Progress " << e.progress << "/"
                << (e.totalUnits > 0 ? std::to_string(e.totalUnits) : "?")
                << ", "
                << "Rating " << e.score << "/10, "
                << "Status " << statusToString(e.status, e.type)
                << (color ? ansi::kReset : "") << "\n";
    }
  }

  if (successCount > 0) {
    db.saveAll();
    return 0;
  }
  return 1;
}

}  // namespace catalist::cli
