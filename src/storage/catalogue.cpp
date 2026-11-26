#include "storage/catalogue.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "core/text.hpp"
#include "storage/csv_codec.hpp"
#include "system.hpp"

namespace catalist {

namespace {

const Entry* FindById(const std::vector<Entry>& list, int id) {
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
	return found == list.end() ? nullptr : &*found;
}

// A local id only numbers a title, so two titles are the same only when a
// site's id says so.
bool SameTitle(const Entry& existing, const Entry& incoming) {
	return (incoming.anilist_id > 0 and existing.anilist_id == incoming.anilist_id) or
	       (incoming.mal_id > 0 and existing.mal_id == incoming.mal_id);
}

int NextId(const std::vector<Entry>& list) {
	auto highest = std::max_element(list.begin(), list.end(),
	                                [](const Entry& a, const Entry& b) { return a.id < b.id; });
	return highest == list.end() ? 1 : highest->id + 1;
}

void StampChanged(std::vector<Entry>& list, const std::vector<Entry>& saved,
                  const std::string& stamp) {
	for (auto& each : list) {
		const Entry* was = FindById(saved, each.id);
		if (not was or not(*was == each))
			each.changed_at = stamp;
	}
}

}  // namespace

Catalogue::Catalogue(std::filesystem::path file) : file_(std::move(file)) {}

std::vector<Entry>& Catalogue::GetList(MediaType type) {
	return type == MediaType::kAnime ? anime_ : manga_;
}

const std::vector<Entry>& Catalogue::GetList(MediaType type) const {
	return type == MediaType::kAnime ? anime_ : manga_;
}

int Catalogue::AddEntry(Entry added) {
	auto& list = GetList(added.type);
	for (auto& existing : list) {
		if (not SameTitle(existing, added))
			continue;
		existing = MergeImported(existing, added);
		return existing.id;
	}
	added.id = NextId(list);
	list.push_back(std::move(added));
	return list.back().id;
}

int Catalogue::ImportEntry(Entry incoming) {
	auto existing = FindImported(incoming);
	if (not existing)
		return AddEntry(std::move(incoming));
	UpdateEntry(MergeImported(*existing, incoming));
	return existing->id;
}

std::optional<Entry> Catalogue::FindImported(const Entry& incoming) const {
	if (incoming.anilist_id > 0 or incoming.mal_id > 0)
		return FindByIds(incoming.type, incoming.anilist_id, incoming.mal_id);
	std::string name = utf8::Lowercase(incoming.title);
	for (const auto& existing : GetList(incoming.type)) {
		bool has_id = existing.anilist_id > 0 or existing.mal_id > 0;
		if (not has_id and utf8::Lowercase(existing.title) == name)
			return existing;
	}
	return std::nullopt;
}

const std::vector<Entry>& Catalogue::GetEntries(MediaType type) const {
	return GetList(type);
}

std::optional<Entry> Catalogue::GetById(MediaType type, int id) const {
	const Entry* found = FindById(GetList(type), id);
	return found ? std::optional<Entry>(*found) : std::nullopt;
}

std::optional<Entry> Catalogue::FindByIds(MediaType type, int anilist_id, int mal_id) const {
	Entry wanted;
	wanted.anilist_id = anilist_id;
	wanted.mal_id = mal_id;
	const auto& list = GetList(type);
	auto found = std::find_if(list.begin(), list.end(),
	                          [&wanted](const Entry& e) { return SameTitle(e, wanted); });
	return found == list.end() ? std::nullopt : std::optional<Entry>(*found);
}

bool Catalogue::UpdateEntry(const Entry& changed) {
	auto& list = GetList(changed.type);
	auto found = std::find_if(list.begin(), list.end(),
	                          [&changed](const Entry& e) { return e.id == changed.id; });
	if (found == list.end())
		return false;
	*found = changed;
	return true;
}

bool Catalogue::RemoveEntry(MediaType type, int id) {
	auto& list = GetList(type);
	auto found =
	    std::find_if(list.begin(), list.end(), [id](const Entry& e) { return e.id == id; });
	if (found == list.end())
		return false;
	list.erase(found);
	return true;
}

Result<bool> Catalogue::Load() {
	anime_.clear();
	manga_.clear();
	savedAnime_.clear();
	savedManga_.clear();
	std::error_code ignored;
	if (not std::filesystem::exists(file_, ignored))
		return false;
	auto text = ReadFile(file_);
	if (not text)
		return std::unexpected(std::format("cannot read {}", file_.string()));

	auto records = csv_codec::SplitRecords(*text);
	if (records.empty() or
	    (records[0] != csv_codec::kHeader and records[0] != csv_codec::kOldHeader))
		return std::unexpected(std::format("{} is not a Catalist list", file_.string()));
	for (size_t i = 1; i < records.size(); ++i) {
		auto row = csv_codec::DecodeEntry(csv_codec::SplitFields(records[i]));
		if (not row or FindById(GetList(row->type), row->id))
			return std::unexpected(
			    std::format("{}: row {} is damaged; fix it or restore the file", file_.string(), i));
		GetList(row->type).push_back(*row);
	}
	savedAnime_ = anime_;
	savedManga_ = manga_;
	return true;
}

bool Catalogue::WriteCsv() const {
	std::string text = std::string(csv_codec::kHeader) + "\n";
	for (const auto* list : {&anime_, &manga_})
		for (const auto& row : *list)
			text += csv_codec::EncodeEntry(row) + "\n";
	return WriteFile(file_, text);
}

bool Catalogue::Save(const std::string& stamp) {
	if (anime_ == savedAnime_ and manga_ == savedManga_)
		return true;
	StampChanged(anime_, savedAnime_, stamp);
	StampChanged(manga_, savedManga_, stamp);
	if (not WriteCsv()) {
		anime_ = savedAnime_;
		manga_ = savedManga_;
		return false;
	}
	savedAnime_ = anime_;
	savedManga_ = manga_;
	return true;
}

}  // namespace catalist
