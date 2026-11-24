#ifndef CATALIST_STORAGE_CATALOGUE_HPP_
#define CATALIST_STORAGE_CATALOGUE_HPP_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/date.hpp"
#include "core/entry.hpp"
#include "core/result.hpp"
#include "storage/journal.hpp"
#include "storage/store.hpp"

namespace catalist {

struct DataDirEnv {
	std::string catalist_data_dir;
	std::string xdg_data_home;
	std::string home;
};

[[nodiscard]] std::filesystem::path DataDirFor(const DataDirEnv& env);

struct TxNote {
	std::string label;
	std::string stamp;
};

struct TxRecord {
	long long id = 0;
	std::string stamp;
	std::string label;
	bool undone = false;
	int changes = 0;
};

struct Replay {
	std::vector<TxRecord> steps;
	std::vector<journal::Change> changes;
};

class Catalogue {
public:
	static constexpr int kHistoryLength = 100;

	explicit Catalogue(std::filesystem::path data_dir);

	int AddEntry(Entry added);
	int ImportEntry(Entry incoming);
	[[nodiscard]] std::optional<Entry> FindImported(const Entry& incoming) const;
	[[nodiscard]] const std::vector<Entry>& GetEntries(MediaType type) const;
	[[nodiscard]] std::optional<Entry> GetById(MediaType type, int id) const;
	[[nodiscard]] std::optional<Entry> FindByIds(MediaType type, int anilist_id, int mal_id) const;

	bool UpdateEntry(const Entry& changed);
	bool RemoveEntry(MediaType type, int id);
	bool RestoreEntry(const Entry& back);

	[[nodiscard]] std::filesystem::path data_dir() const;
	[[nodiscard]] std::filesystem::path StorePath() const;

	[[nodiscard]] Done BeginWrite();

	[[nodiscard]] Result<bool> LoadAll();
	[[nodiscard]] bool SaveAll();
	[[nodiscard]] bool SaveAll(const TxNote& note);

	[[nodiscard]] Result<Replay> Undo(int count, bool force);
	[[nodiscard]] Result<Replay> Redo(int count, bool force);
	[[nodiscard]] Result<std::vector<TxRecord>> History(int limit);

private:
	std::vector<Entry>& GetList(MediaType type);
	const std::vector<Entry>& GetList(MediaType type) const;
	const std::vector<Entry>& GetSaved(MediaType type) const;

	[[nodiscard]] Done OpenStore();
	[[nodiscard]] Done SetUpStore();
	[[nodiscard]] bool ReadStore();
	[[nodiscard]] bool WriteRows();
	[[nodiscard]] Result<Replay> WalkHistory(bool redoing, int count, bool force);
	[[nodiscard]] std::string ReplayStep(const TxRecord& step, bool redoing, bool force);
	[[nodiscard]] bool Commit(const std::function<bool()>& work);
	void RememberSaved();

	std::filesystem::path dataDir_;
	std::vector<Entry> animeEntries_;
	std::vector<Entry> mangaEntries_;

	std::vector<Entry> savedAnime_;
	std::vector<Entry> savedManga_;

	store::Database db_;
	std::optional<store::Transaction> write_;
};

}  // namespace catalist

#endif  // CATALIST_STORAGE_CATALOGUE_HPP_
