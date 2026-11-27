#include <format>

#include "core/text.hpp"
#include "menu/common.hpp"
#include "menu/input.hpp"
#include "menu/output.hpp"
#include "web/anilist.hpp"

namespace catalist {

namespace {

const std::string& ShownName(const anilist::Match& match) {
	return match.english.empty() ? match.name : match.english;
}

Entry FromMatch(const anilist::Match& match) {
	Entry made;
	made.type = match.type;
	made.title = match.name;
	made.english_title = match.english == match.name ? "" : match.english;
	made.anilist_id = match.anilist_id;
	made.mal_id = match.mal_id;
	made.total_units = match.total_units;
	made.total_volumes = match.total_volumes;
	made.format = match.format;
	made.status = WatchStatus::kPlanToWatch;
	return made;
}

std::string LengthOf(const anilist::Match& match) {
	return std::format("{} {}", match.total_units > 0 ? std::to_string(match.total_units) : "?",
	                   UnitName(match.type, match.total_units));
}

std::optional<anilist::Match> OnlyExact(const std::vector<anilist::Match>& matches,
                                        const std::string& query) {
	std::string wanted = utf8::Folded(query);
	std::vector<anilist::Match> exact;
	for (const auto& match : matches)
		if (utf8::Folded(match.name) == wanted or utf8::Folded(match.english) == wanted)
			exact.push_back(match);
	return exact.size() == 1 ? std::optional(exact[0]) : std::nullopt;
}

void PrintMatches(App& app, MediaType type, const std::vector<anilist::Match>& matches) {
	bool anime = type == MediaType::kAnime;
	int digits = static_cast<int>(std::to_string(matches.size()).size());
	std::vector<std::vector<output::Styled>> rows;
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		auto mine = app.db.FindByIds(match.type, match.anilist_id, match.mal_id);
		std::vector<output::Styled> row = {{std::format("  {:>{}}", i + 1, digits), output::kCyan},
		                                   {match.year.empty() ? "-" : match.year}};
		if (anime)
			row.push_back({MediaFormatLabel(match.format)});
		row.push_back({ShownName(match)});
		row.push_back({LengthOf(match)});
		row.push_back({output::Stars(match.score), output::kYellow});
		row.push_back({mine ? std::format("{} {}", StatusMark(mine->status),
		                                  StatusLabel(mine->status, match.type))
		                    : "",
		               mine ? StatusColor(mine->status) : nullptr});
		rows.push_back(row);
	}
	std::vector<output::TableColumn> columns = {{}, {}};
	if (anime)
		columns.push_back({});
	columns.insert(columns.end(), {{.flexible = true}, {.align_right = true}, {}, {}});
	app.out << output::Rows(columns, rows, app.columns, app.color);
}

std::optional<anilist::Match> ChooseMatch(App& app, MediaType type,
                                          const std::vector<anilist::Match>& matches,
                                          const std::string& query) {
	if (matches.size() == 1)
		return matches[0];
	if (auto exact = OnlyExact(matches, query))
		return exact;
	PrintMatches(app, type, matches);
	app.out << "\n";
	DrawOption(app, 0, static_cast<int>(std::to_string(matches.size()).size()), "Back");
	app.out << "\n";
	int choice = ReadChoice(app, static_cast<int>(matches.size()));
	if (choice == 0)
		return std::nullopt;
	return matches[static_cast<size_t>(choice - 1)];
}

void SaveNewTitle(App& app, const anilist::Match& match, WatchStatus status) {
	IsoDate today = app.Today();
	Entry added = FromMatch(match);
	added.updated_at = today;
	added = SetStatus(added, status, today);
	app.db.AddEntry(added);
	if (not app.db.Save(app.Stamp())) {
		Fail(app, std::format("could not save {}", app.db.file().string()));
		return;
	}
	Done(app,
	     std::format("{} added as {}.", DisplayName(added), StatusLabel(added.status, added.type)));
}

}  // namespace

void AddScreen(App& app, MediaType type) {
	std::string crumb = std::format("Catalist / {} / Add", TypeToString(type));
	ClearScreen(app);
	DrawHeading(app, crumb);
	auto name =
	    AskText(app, std::format("{} Search AniList for {} (Enter to go back): ",
		                         output::Paint(">", output::kMagenta, app.color), TypeKey(type)));
	if (not name)
		return;
	auto found = anilist::Search(app.net, type, *name);
	if (not found.has_value()) {
		Fail(app, found.error());
		Pause(app);
		return;
	}
	if (found.value().empty()) {
		Note(app, std::format("AniList has no {} matching '{}'.", TypeKey(type), *name));
		Pause(app);
		return;
	}
	auto match = ChooseMatch(app, type, found.value(), *name);
	if (not match)
		return;
	if (auto mine = app.db.FindByIds(match->type, match->anilist_id, match->mal_id)) {
		Note(app, std::format("{} is in your {} list already ({}).", DisplayName(*mine),
		                      TypeKey(mine->type), StatusLabel(mine->status, mine->type)));
		Pause(app);
		return;
	}
	ClearScreen(app);
	auto status = AskStatus(app, std::format("{} / {}", crumb, ShownName(*match)),
	                        kStatusesForNewTitle, match->type);
	if (not status)
		return;
	SaveNewTitle(app, *match, *status);
	Pause(app);
}

}  // namespace catalist
