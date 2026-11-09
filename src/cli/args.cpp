#include "catalist/cli/args.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "catalist/cli/output.hpp"

namespace catalist::cli {

namespace {

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

std::string trimQuotesAndSpace(std::string text) {
  const char* noise = " \t\r\n\"";
  text.erase(0, text.find_first_not_of(noise));
  text.erase(text.find_last_not_of(noise) + 1);
  return text;
}

std::optional<int> parseInt(const std::string& text) {
  try {
    return std::stoi(text);
  } catch (...) {
    return std::nullopt;
  }
}

bool isScore(int score) {
  return score >= 0 && score <= 10;
}

bool startsWith(const std::string& text, const char* prefix) {
  return text.rfind(prefix, 0) == 0;
}

std::optional<ratingFilter> window(std::optional<int> low,
                                   std::optional<int> high) {
  if (!low || !high || !isScore(*low) || !isScore(*high) || *low > *high)
    return std::nullopt;
  return ratingFilter{*low, *high, false};
}

std::optional<sortKey> parseSortKey(const std::string& raw) {
  std::string key = lowercase(raw);
  if (key == "score" || key == "rating")
    return sortKey::kScore;
  if (key == "title")
    return sortKey::kTitle;
  if (key == "status")
    return sortKey::kStatus;
  if (key == "progress")
    return sortKey::kProgress;
  if (key == "date" || key == "started" || key == "chrono")
    return sortKey::kDate;
  if (key == "id")
    return sortKey::kId;
  return std::nullopt;
}

bool takesValue(const std::string& flag) {
  static const char* valued[] = {"--type",   "-t",     "--status", "-s",
                                 "--rating", "-r",     "--score",  "--year",
                                 "-y",       "--search", "-q",     "--limit",
                                 "-l",       "--sort"};
  return std::any_of(std::begin(valued), std::end(valued),
                     [&](const char* name) { return flag == name; });
}

std::string canonicalName(const std::string& flag) {
  if (flag == "-t")
    return "--type";
  if (flag == "-s")
    return "--status";
  if (flag == "-r" || flag == "--score")
    return "--rating";
  if (flag == "-y")
    return "--year";
  if (flag == "-q")
    return "--search";
  if (flag == "-l")
    return "--limit";
  return flag;
}

std::optional<std::string> applySwitch(const std::string& flag,
                                       cliFilterOptions& opts) {
  if (flag == "--json") {
    opts.format = outputFormat::kJson;
    opts.jsonErrors = true;
  } else if (flag == "--plain") {
    opts.format = outputFormat::kPlain;
  } else if (flag == "--ids") {
    opts.format = outputFormat::kIds;
  } else if (flag == "--csv") {
    opts.format = outputFormat::kCsv;
  } else if (flag == "--reverse" || flag == "-R") {
    opts.reverseSort = true;
  } else if (flag == "--unrated") {
    opts.rating = ratingFilter{0, 0, true};
  } else {
    return "unknown flag '" + flag + "'.";
  }
  return std::nullopt;
}

std::optional<std::string> applyValue(const std::string& name,
                                      const std::string& value,
                                      cliFilterOptions& opts) {
  if (name == "--type") {
    opts.type = parseMediaTypeArgument(value);
    if (!opts.type)
      return "invalid media type '" + value + "'. Expected 'anime' or 'manga'.";
  } else if (name == "--status") {
    opts.status = parseStatus(value);
    if (!opts.status)
      return "invalid status '" + value + "'. " + kStatusChoices;
  } else if (name == "--rating") {
    opts.rating = parseRatingSpec(value);
    if (!opts.rating)
      return "invalid rating specification '" + value +
             "'. Expected e.g. 9, '>=9', '<=7', '6..8', or 'unrated'.";
  } else if (name == "--year") {
    opts.year = value.empty() ? std::nullopt : std::optional<std::string>(value);
  } else if (name == "--search") {
    opts.search = value;
  } else if (name == "--limit") {
    auto limit = parseInt(value);
    if (!limit)
      return "invalid integer for '--limit': " + value;
    if (*limit < 0)
      return std::string("--limit must be non-negative");
    opts.limit = static_cast<size_t>(*limit);
  } else {
    auto key = parseSortKey(value);
    if (!key)
      return "unknown sort key '" + value +
             "'. Expected: rating, title, status, progress, date, id.";
    opts.sortBy = *key;
  }
  return std::nullopt;
}

bool matchesFilters(const entry& e, const cliFilterOptions& opts,
                    const std::string& loweredSearch) {
  if (opts.status && e.status != *opts.status)
    return false;
  if (opts.rating &&
      (e.score < opts.rating->minScore || e.score > opts.rating->maxScore))
    return false;
  if (opts.year && (!e.startDate ||
                    formatDate(e.startDate).substr(0, 4) != *opts.year))
    return false;
  return loweredSearch.empty() ||
         lowercase(e.title).find(loweredSearch) != std::string::npos;
}

}  // namespace

std::optional<ratingFilter> parseRatingSpec(const std::string& spec) {
  std::string s = trimQuotesAndSpace(spec);
  if (s == "unrated" || s == "0")
    return ratingFilter{0, 0, true};
  if (startsWith(s, ">="))
    return window(parseInt(s.substr(2)), 10);
  if (startsWith(s, ">")) {
    auto bound = parseInt(s.substr(1));
    return window(bound ? std::optional<int>(*bound + 1) : std::nullopt, 10);
  }
  if (startsWith(s, "<="))
    return window(0, parseInt(s.substr(2)));
  if (startsWith(s, "<")) {
    auto bound = parseInt(s.substr(1));
    return window(0, bound ? std::optional<int>(*bound - 1) : std::nullopt);
  }
  size_t dots = s.find("..");
  if (dots != std::string::npos)
    return window(parseInt(s.substr(0, dots)), parseInt(s.substr(dots + 2)));
  auto exact = parseInt(s);
  return window(exact, exact);
}

std::optional<mediaType> parseMediaTypeArgument(const std::string& raw) {
  std::string kind = lowercase(raw);
  if (kind == "anime" || kind == "a")
    return mediaType::kAnime;
  if (kind == "manga" || kind == "m")
    return mediaType::kManga;
  return std::nullopt;
}

result<cliFilterOptions, argumentError> parseCommonFilters(
    const std::vector<std::string>& args, cliFilterOptions defaults) {
  using outcome = result<cliFilterOptions, argumentError>;
  cliFilterOptions opts = std::move(defaults);
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& flag = args[i];
    std::optional<std::string> problem;
    if (!takesValue(flag)) {
      problem = applySwitch(flag, opts);
    } else if (i + 1 >= args.size()) {
      problem = "missing argument for '" + canonicalName(flag) + "'";
    } else {
      problem = applyValue(canonicalName(flag), args[++i], opts);
    }
    if (problem)
      return outcome::failure({*problem, opts.jsonErrors});
  }
  return outcome::success(std::move(opts));
}

std::vector<entry> collectAndFilterEntries(const database& db,
                                           const cliFilterOptions& opts) {
  std::string loweredSearch = lowercase(opts.search);
  std::vector<entry> pool;
  for (auto kind : {mediaType::kAnime, mediaType::kManga}) {
    if (opts.type && *opts.type != kind)
      continue;
    for (const auto& e : db.getEntries(kind)) {
      if (matchesFilters(e, opts, loweredSearch))
        pool.push_back(e);
    }
  }
  database::sortEntries(pool, opts.sortBy, opts.reverseSort);
  if (opts.limit && *opts.limit < pool.size())
    pool.resize(*opts.limit);
  return pool;
}

std::vector<std::string> collectQueries(const std::string& target) {
  if (target != "-")
    return {target};
  std::vector<std::string> queries;
  std::string line;
  while (std::getline(std::cin, line)) {
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);
    if (!line.empty())
      queries.push_back(line);
  }
  return queries;
}

std::optional<std::optional<isoDate>> parseDateArgument(const std::string& raw,
                                                        const std::string& flag,
                                                        bool jsonMode) {
  if (raw.empty())
    return std::optional<isoDate>{};
  auto parsed = isoDate::parse(raw);
  if (parsed)
    return std::optional<isoDate>{parsed};
  emitError("usage",
            "invalid date '" + raw + "' for '" + flag +
                "'. Expected YYYY-MM-DD.",
            jsonMode);
  return std::nullopt;
}

}  // namespace catalist::cli
