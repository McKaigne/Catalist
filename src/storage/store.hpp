#ifndef CATALIST_STORAGE_STORE_HPP_
#define CATALIST_STORAGE_STORE_HPP_

#include <filesystem>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace catalist::store {

class Statement {
public:
	Statement(sqlite3* db, const std::string& sql);
	~Statement();
	Statement(Statement&& other) noexcept;
	Statement(const Statement&) = delete;
	Statement& operator=(const Statement&) = delete;
	Statement& operator=(Statement&&) = delete;

	Statement& Bind(long long value);
	Statement& Bind(int value) { return Bind(static_cast<long long>(value)); }
	Statement& Bind(const std::string& value);
	Statement& BindNull();

	[[nodiscard]] bool Step();
	[[nodiscard]] bool Run();

	[[nodiscard]] bool Ok() const { return not failed_; }

	[[nodiscard]] long long IntAt(int column) const;
	[[nodiscard]] std::string TextAt(int column) const;

private:
	sqlite3_stmt* handle_ = nullptr;
	int nextParameter_ = 1;
	bool failed_ = false;
};

class Database {
public:
	Database() = default;
	~Database();
	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;

	[[nodiscard]] bool Open(const std::filesystem::path& file);
	void Close();
	[[nodiscard]] bool IsOpen() const { return handle_ != nullptr; }

	[[nodiscard]] bool Run(const std::string& sql);
	[[nodiscard]] Statement Prepare(const std::string& sql) const;
	[[nodiscard]] long long LastInsertId() const;
	[[nodiscard]] std::string LastError() const;

private:
	sqlite3* handle_ = nullptr;
	std::string openError_ = "it is not open";
};

class Transaction {
public:
	explicit Transaction(Database& db);
	~Transaction();
	Transaction(const Transaction&) = delete;
	Transaction& operator=(const Transaction&) = delete;

	[[nodiscard]] bool Active() const { return active_; }
	[[nodiscard]] bool Commit();

private:
	Database& db_;
	bool active_ = false;
};

}  // namespace catalist::store

#endif  // CATALIST_STORAGE_STORE_HPP_
