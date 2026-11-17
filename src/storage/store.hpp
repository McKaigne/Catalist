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
class statement {
public:
	statement(sqlite3* db, const std::string& sql);
	~statement();
	statement(statement&& other) noexcept;
	statement(const statement&) = delete;
	statement& operator=(const statement&) = delete;
	statement& operator=(statement&&) = delete;

	statement& bind(long long value);
	statement& bind(int value) { return bind(static_cast<long long>(value)); }
	statement& bind(const std::string& value);
	statement& bindNull();

	// Moves to the next row; false when there is none (or on an error).
	[[nodiscard]] bool step();
	// Runs a statement that returns no rows, then readies it for new
	// parameters, so one statement can write many rows.
	[[nodiscard]] bool run();
	// Readies a query for new parameters, to be asked again.
	void reset();

	// False once preparing, binding or stepping has failed.
	[[nodiscard]] bool ok() const { return not failed_; }

	[[nodiscard]] long long intAt(int column) const;
	// Empty for NULL.
	[[nodiscard]] std::string textAt(int column) const;

private:
	sqlite3_stmt* handle_ = nullptr;
	int nextParameter_ = 1;
	bool failed_ = false;
};

enum class openMode { kReadWrite, kReadOnly };

class database {
public:
	database() = default;
	~database();
	database(const database&) = delete;
	database& operator=(const database&) = delete;

	// Opens the file, creating it unless read-only. Another program's write
	// is waited for up to `busyWaitMs`.
	[[nodiscard]] bool open(const std::filesystem::path& file, openMode mode = openMode::kReadWrite,
	                        int busyWaitMs = 5000);
	void close();
	[[nodiscard]] bool isOpen() const { return handle_ != nullptr; }

	// Runs statements that take no parameters and return nothing.
	[[nodiscard]] bool run(const std::string& sql);
	[[nodiscard]] statement prepare(const std::string& sql) const;
	// The row id the last INSERT made.
	[[nodiscard]] long long lastInsertId() const;
	// Why the last call failed, in SQLite's words.
	[[nodiscard]] std::string lastError() const;

private:
	sqlite3* handle_ = nullptr;
	std::string openError_ = "it is not open";
};

// A write transaction (BEGIN IMMEDIATE): other writers wait from here on,
// readers carry on until the commit.
class transaction {
public:
	explicit transaction(database& db);
	~transaction();
	transaction(const transaction&) = delete;
	transaction& operator=(const transaction&) = delete;

	// False when the lock could not be taken, and after the end.
	[[nodiscard]] bool active() const { return active_; }
	[[nodiscard]] bool commit();

private:
	database& db_;
	bool active_ = false;
};

}  // namespace catalist::store

#endif  // CATALIST_STORAGE_STORE_HPP_
