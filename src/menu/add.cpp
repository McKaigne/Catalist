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

void PrintMatches(App& app, const std::vector<anilist::Match>& matches) {
	std::vector<output::Section> sections = {
	    {std::format("{} Anime", output::icon::kAnime), {}},
	    {std::format("{} Manga", output::icon::kManga), {}}};
	int digits = static_cast<int>(std::to_string(matches.size()).size());
	for (size_t i = 0; i < matches.size(); ++i) {
		const auto& match = matches[i];
		auto mine = app.db.FindByIds(match.type, match.anilist_id, match.mal_id);
		sections[match.type == MediaType::kAnime ? 0 : 1].rows.push_back(
		    {{"  " + output::NumberHint(static_cast<int>(i + 1), digits), output::kCyan},
		     {match.year.empty() ? "-" : match.year},
		     {ShownName(match)},
		     {LengthOf(match)},
		     {output::Stars(match.score), output::kYellow},
		     {mine ? std::format("{} {}", StatusIcon(mine->status),
		                         StatusLabel(mine->status, match.type))
		           : "",
		      mine ? StatusColor(mine->status) : nullptr}});
	}
	app.out << output::Sections(
	    {{}, {}, {.flexible = true}, {.align_right = true}, {}, {}}, sections, app.columns,
	    app.color);
}

std::optional<anilist::Match> ChooseMatch(App& app, const std::vector<anilist::Match>& matches,
                                          const std::string& query) {
	if (matches.size() == 1)
		return matches[0];
	if (auto exact = OnlyExact(matches, query))
		return exact;
	PrintMatches(app, matches);
	app.out << "\n";
	DrawOption(app, 0, static_cast<int>(std::to_string(matches.size()).size()), kBackOption);
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
	Done(app, std::format("{} added as {}.", DisplayName(added),
	                      StatusLabel(added.status, added.type)));
}

}  // namespace

void AddScreen(App& app) {
	ClearScreen(app);
	DrawMenu(app, "Catalist / Add Title",
	         {{output::icon::kAddBoth, "Both"},
	          {output::icon::kAnime, "Anime Only"},
	          {output::icon::kManga, "Manga Only"}});
	int where = ReadChoice(app, 3);
	if (where == 0)
		return;
	std::optional<MediaType> only;
	if (where == 2)
		only = MediaType::kAnime;
	else if (where == 3)
		only = MediaType::kManga;

	auto name = AskText(app, std::format("{} Search AniList (Enter to go back): ",
	                                     output::Paint(output::icon::kSearch, output::kMagenta,
	                                                   app.color)));
	if (not name)
		return;
	auto found = anilist::Search(app.net, only, *name);
	if (not found.has_value()) {
		Fail(app, found.error());
		Pause(app);
		return;
	}
	if (found.value().empty()) {
		app.out << std::format("{} AniList has no title matching '{}'.\n", output::icon::kInfo,
		                       *name);
		Pause(app);
		return;
	}
	auto match = ChooseMatch(app, found.value(), *name);
	if (not match)
		return;
	if (auto mine = app.db.FindByIds(match->type, match->anilist_id, match->mal_id)) {
		app.out << std::format("{} {} is in your {} list already ({}).\n", output::icon::kInfo,
		                       DisplayName(*mine),
		                       TypeKey(mine->type), StatusLabel(mine->status, mine->type));
		Pause(app);
		return;
	}
	ClearScreen(app);
	auto status = AskStatus(app, std::format("Catalist / Add Title / {}", ShownName(*match)),
	                        kStatusesForNewTitle, match->type);
	if (not status)
		return;
	SaveNewTitle(app, *match, *status);
	Pause(app);
}

}  // namespace catalist
