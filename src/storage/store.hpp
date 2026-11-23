#ifndef CATALIST_STORAGE_STORE_HPP_
#define CATALIST_STORAGE_STORE_HPP_

#include <filesystem>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

// A thin layer over SQLite's C interface, the only code that calls it:
// handles are closed and statements finalized when their owners go out of
// scope, and an open transaction is rolled back unless it was committed.
namespace catalist::store {

// One prepared statement. Parameters are bound left to right; a failure at
// any point makes `step` and `run` return false.
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

	// Moves to the next row; false when there is none (or on an error).
	[[nodiscard]] bool Step();
	// Runs a statement that returns no rows, then readies it for new
	// parameters, so one statement can write many rows.
	[[nodiscard]] bool Run();
	// Readies a query for new parameters, to be asked again.
	void Reset();

	// False once preparing, binding or stepping has failed.
	[[nodiscard]] bool Ok() const { return not failed_; }

	[[nodiscard]] long long IntAt(int column) const;
	// Empty for NULL.
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

	// Opens the file, creating it if need be. Another program's write is
	// waited for up to five seconds.
	[[nodiscard]] bool Open(const std::filesystem::path& file);
	void Close();
	[[nodiscard]] bool IsOpen() const { return handle_ != nullptr; }

	// Runs statements that take no parameters and return nothing.
	[[nodiscard]] bool Run(const std::string& sql);
	[[nodiscard]] Statement Prepare(const std::string& sql) const;
	// The row id the last INSERT made.
	[[nodiscard]] long long LastInsertId() const;
	// Why the last call failed, in SQLite's words.
	[[nodiscard]] std::string LastError() const;

private:
	sqlite3* handle_ = nullptr;
	std::string openError_ = "it is not open";
};

// A write transaction (BEGIN IMMEDIATE): other writers wait from here on,
// readers carry on until the commit.
class Transaction {
public:
	explicit Transaction(Database& db);
	~Transaction();
	Transaction(const Transaction&) = delete;
	Transaction& operator=(const Transaction&) = delete;

	// False when the lock could not be taken, and after the end.
	[[nodiscard]] bool Active() const { return active_; }
	[[nodiscard]] bool Commit();

private:
	Database& db_;
	bool active_ = false;
};

}  // namespace catalist::store

#endif  // CATALIST_STORAGE_STORE_HPP_
