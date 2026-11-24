#ifndef CATALIST_STORAGE_CATALOGUE_HPP_
#define CATALIST_STORAGE_CATALOGUE_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "core/result.hpp"

namespace catalist {

class Catalogue {
public:
	explicit Catalogue(std::filesystem::path file);

	int AddEntry(Entry added);
	int ImportEntry(Entry incoming);
	[[nodiscard]] std::optional<Entry> FindImported(const Entry& incoming) const;
	[[nodiscard]] const std::vector<Entry>& GetEntries(MediaType type) const;
	[[nodiscard]] std::optional<Entry> GetById(MediaType type, int id) const;
	[[nodiscard]] std::optional<Entry> FindByIds(MediaType type, int anilist_id, int mal_id) const;
	bool UpdateEntry(const Entry& changed);
	bool RemoveEntry(MediaType type, int id);

	[[nodiscard]] const std::filesystem::path& file() const { return file_; }

	// False when there is no file yet. Fails on a damaged file, naming the row.
	[[nodiscard]] Result<bool> Load();
	// Writes the CSV. When the write fails, the lists go back to what the file
	// holds.
	[[nodiscard]] bool Save(const std::string& stamp);

private:
	std::vector<Entry>& GetList(MediaType type);
	const std::vector<Entry>& GetList(MediaType type) const;
	[[nodiscard]] bool WriteCsv() const;

	std::filesystem::path file_;
	std::vector<Entry> anime_;
	std::vector<Entry> manga_;
	std::vector<Entry> savedAnime_;
	std::vector<Entry> savedManga_;
};

}  // namespace catalist

#endif  // CATALIST_STORAGE_CATALOGUE_HPP_
