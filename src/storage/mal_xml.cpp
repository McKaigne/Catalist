#include "storage/mal_xml.hpp"

#include <algorithm>
#include <cctype>
#include <format>

namespace catalist::mal_xml {

namespace {

std::string DecodeXmlEntities(std::string text) {
	auto replace_all = [&](const std::string& from, const std::string& to) {
		size_t pos = 0;
		while ((pos = text.find(from, pos)) != std::string::npos) {
			text.replace(pos, from.length(), to);
			pos += to.length();
		}
	};
	replace_all("&lt;", "<");
	replace_all("&gt;", ">");
	replace_all("&quot;", "\"");
	replace_all("&#039;", "'");
	replace_all("&apos;", "'");
	// Last, so "&amp;lt;" stays the text "&lt;".
	replace_all("&amp;", "&");
	return text;
}

std::string TagText(const std::string& block, const std::string& tag) {
	std::string open_tag = std::format("<{}>", tag);
	std::string close_tag = std::format("</{}>", tag);
	size_t start = block.find(open_tag);
	if (start == std::string::npos)
		return "";
	start += open_tag.length();
	size_t end = block.find(close_tag, start);
	if (end == std::string::npos)
		return "";

	std::string text = block.substr(start, end - start);
	const std::string cdata_open = "<![CDATA[";
	size_t cdata_start = text.find(cdata_open);
	size_t cdata_end = text.rfind("]]>");
	if (cdata_start != std::string::npos and cdata_end != std::string::npos and
	    cdata_end >= cdata_start + cdata_open.length()) {
		cdata_start += cdata_open.length();
		text = text.substr(cdata_start, cdata_end - cdata_start);
		const std::string seam = "]]]]><![CDATA[>";
		for (size_t at = text.find(seam); at != std::string::npos; at = text.find(seam, at + 3))
			text.replace(at, seam.length(), "]]>");
		return text;
	}

	size_t first = text.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";
	size_t last = text.find_last_not_of(" \t\r\n");
	return DecodeXmlEntities(text.substr(first, last - first + 1));
}

int TagNumber(const std::string& block, const std::string& tag) {
	std::string digits = TagText(block, tag);
	if (digits.length() > 8)
		return 0;
	int number = 0;
	for (char c : digits) {
		if (c < '0' or c > '9')
			return 0;
		number = number * 10 + (c - '0');
	}
	return number;
}

// my_rewatching holds 1 or 0 for anime, my_rereading YES or NO for manga.
enum class RepeatFlag { kNo, kYes };

RepeatFlag ParseRepeatFlag(const std::string& text) {
	std::string word = text;
	std::transform(word.begin(), word.end(), word.begin(),
	               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return word == "1" or word == "YES" or word == "TRUE" ? RepeatFlag::kYes : RepeatFlag::kNo;
}

std::string RepeatFlagText(RepeatFlag flag, MediaType type) {
	bool yes = flag == RepeatFlag::kYes;
	if (type == MediaType::kAnime)
		return yes ? "1" : "0";
	return yes ? "YES" : "NO";
}

RepeatFlag FlagOf(const Entry& item) {
	return item.status == WatchStatus::kRewatching ? RepeatFlag::kYes : RepeatFlag::kNo;
}

// series_type also holds CM, PV and Unknown, which Catalist has no format for.
MediaFormat ParseSeriesType(const std::string& text) {
	const std::pair<const char*, MediaFormat> types[] = {
	    {"TV", MediaFormat::kTv},           {"TV Special", MediaFormat::kTvSpecial},
	    {"Movie", MediaFormat::kMovie},     {"Special", MediaFormat::kSpecial},
	    {"OVA", MediaFormat::kOva},         {"ONA", MediaFormat::kOna},
	    {"Music", MediaFormat::kMusic}};
	for (const auto& [name, format] : types)
		if (text == name)
			return format;
	return MediaFormat::kUnknown;
}

std::string SeriesType(MediaFormat format) {
	switch (format) {
		case MediaFormat::kUnknown:
			return "Unknown";
		case MediaFormat::kTvShort:
			return "TV";
		default:
			return MediaFormatLabel(format);
	}
}

struct BlockTags {
	MediaType type;
	const char* block;
	const char* id;
	const char* title;
	const char* total;
	const char* progress;
	const char* repeats;
	const char* again;
};

const BlockTags kAnimeTags = {MediaType::kAnime,  "anime",           "series_animedb_id",
                              "series_title",     "series_episodes", "my_watched_episodes",
                              "my_times_watched", "my_rewatching"};
const BlockTags kMangaTags = {MediaType::kManga, "manga",          "manga_mangadb_id",
                              "manga_title",     "manga_chapters", "my_read_chapters",
                              "my_times_read",   "my_rereading"};

void ParseBlocks(const std::string& xml, const BlockTags& tags, std::vector<Entry>& found) {
	std::string open_tag = std::format("<{}>", tags.block);
	std::string close_tag = std::format("</{}>", tags.block);
	size_t at = 0;
	while ((at = xml.find(open_tag, at)) != std::string::npos) {
		size_t end = xml.find(close_tag, at);
		if (end == std::string::npos)
			break;
		std::string block = xml.substr(at, end - at);
		at = end + close_tag.length();

		Entry item;
		item.type = tags.type;
		item.title = TagText(block, tags.title);
		if (item.title.empty())
			continue;
		item.mal_id = TagNumber(block, tags.id);
		item.total_units = TagNumber(block, tags.total);
		item.progress = TagNumber(block, tags.progress);
		item.rewatched = TagNumber(block, tags.repeats);
		item.total_volumes = TagNumber(block, "manga_volumes");
		item.volumes = TagNumber(block, "my_read_volumes");
		item.score = std::min(TagNumber(block, "my_score"), 10);
		item.status = ParseStatus(TagText(block, "my_status")).value_or(WatchStatus::kPlanToWatch);
		if (ParseRepeatFlag(TagText(block, tags.again)) == RepeatFlag::kYes)
			item.status = WatchStatus::kRewatching;
		if (tags.type == MediaType::kAnime)
			item.format = ParseSeriesType(TagText(block, "series_type"));
		// MyAnimeList writes 0000-00-00 for "no date", which parses as absent.
		item.start_date = IsoDate::Parse(TagText(block, "my_start_date"));
		if (item.status == WatchStatus::kCompleted or item.status == WatchStatus::kRewatching)
			item.finish_date = IsoDate::Parse(TagText(block, "my_finish_date"));
		found.push_back(item);
	}
}

// CDATA cannot contain "]]>", so it is split across two sections.
std::string Cdata(const std::string& text) {
	std::string escaped;
	size_t start = 0;
	for (size_t found = text.find("]]>"); found != std::string::npos;
	     found = text.find("]]>", start)) {
		escaped += std::format("{}]]]]><![CDATA[>", text.substr(start, found - start));
		start = found + 3;
	}
	return std::format("<![CDATA[{}{}]]>", escaped, text.substr(start));
}

std::string XmlTag(const std::string& name, const std::string& value) {
	return std::format("\t\t<{0}>{1}</{0}>\n", name, value);
}

// MyAnimeList has no status for a rewatch: it is watching, with
// my_rewatching set.
std::string XmlStatus(const Entry& item) {
	bool again = item.status == WatchStatus::kRewatching;
	return StatusToString(again ? WatchStatus::kWatching : item.status, item.type);
}

std::string AnimeBlock(const Entry& item) {
	std::string block = "\t<anime>\n";
	block += XmlTag("series_animedb_id", std::to_string(item.mal_id));
	block += XmlTag("series_title", Cdata(item.title));
	block += XmlTag("series_type", SeriesType(item.format));
	block += XmlTag("series_episodes", std::to_string(item.total_units));
	block += XmlTag("my_watched_episodes", std::to_string(item.progress));
	block += XmlTag("my_times_watched", std::to_string(item.rewatched));
	block += XmlTag("my_rewatching", RepeatFlagText(FlagOf(item), item.type));
	return block;
}

std::string MangaBlock(const Entry& item) {
	std::string block = "\t<manga>\n";
	block += XmlTag("manga_mangadb_id", std::to_string(item.mal_id));
	block += XmlTag("manga_title", Cdata(item.title));
	block += XmlTag("manga_volumes", std::to_string(item.total_volumes));
	block += XmlTag("manga_chapters", std::to_string(item.total_units));
	block += XmlTag("my_read_volumes", std::to_string(item.volumes));
	block += XmlTag("my_read_chapters", std::to_string(item.progress));
	block += XmlTag("my_times_read", std::to_string(item.rewatched));
	block += XmlTag("my_rereading", RepeatFlagText(FlagOf(item), item.type));
	return block;
}

std::string EntryBlock(const Entry& item) {
	bool anime = item.type == MediaType::kAnime;
	std::string block = anime ? AnimeBlock(item) : MangaBlock(item);
	if (item.start_date)
		block += XmlTag("my_start_date", item.start_date->ToString());
	if (item.finish_date)
		block += XmlTag("my_finish_date", item.finish_date->ToString());
	block += XmlTag("my_score", std::to_string(item.score));
	block += XmlTag("my_status", XmlStatus(item));
	block += XmlTag("update_on_import", "1");
	return block + (anime ? "\t</anime>\n" : "\t</manga>\n");
}

}  // namespace

std::vector<Entry> Parse(const std::string& xml) {
	std::vector<Entry> found;
	ParseBlocks(xml, kAnimeTags, found);
	ParseBlocks(xml, kMangaTags, found);
	return found;
}

Exported Write(const std::vector<Entry>& entries, MediaType type) {
	Exported result;
	result.xml = std::format(
	    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<myanimelist>\n"
	    "\t<myinfo>\n\t\t<user_export_type>{}</user_export_type>\n"
	    "\t</myinfo>\n",
	    type == MediaType::kAnime ? 1 : 2);
	for (const auto& item : entries) {
		if (item.type != type)
			continue;
		if (item.mal_id <= 0) {
			++result.skipped;
			continue;
		}
		result.xml += EntryBlock(item);
		++result.written;
	}
	result.xml += "</myanimelist>\n";
	return result;
}

}  // namespace catalist::mal_xml
