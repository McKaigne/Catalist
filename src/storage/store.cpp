#include "storage/store.hpp"

#include <sqlite3.h>

#include <utility>

namespace catalist::store {

statement::statement(sqlite3* db, const std::string& sql) {
	failed_ = db == nullptr or sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()),
	                                              &handle_, nullptr) != SQLITE_OK;
}

statement::~statement() {
	sqlite3_finalize(handle_);
}

statement::statement(statement&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)),
      nextParameter_(other.nextParameter_),
      failed_(other.failed_) {}

statement& statement::bind(long long value) {
	if (not failed_)
		failed_ = sqlite3_bind_int64(handle_, nextParameter_++, value) != SQLITE_OK;
	return *this;
}

statement& statement::bind(const std::string& value) {
	if (not failed_)
		failed_ = sqlite3_bind_text(handle_, nextParameter_++, value.data(),
		                            static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK;
	return *this;
}

statement& statement::bindNull() {
	if (not failed_)
		failed_ = sqlite3_bind_null(handle_, nextParameter_++) != SQLITE_OK;
	return *this;
}

bool statement::step() {
	if (failed_)
		return false;
	int state = sqlite3_step(handle_);
	failed_ = state != SQLITE_ROW and state != SQLITE_DONE;
	return state == SQLITE_ROW;
}

bool statement::run() {
	if (failed_)
		return false;
	failed_ = sqlite3_step(handle_) != SQLITE_DONE;
	sqlite3_reset(handle_);
	sqlite3_clear_bindings(handle_);
	nextParameter_ = 1;
	return not failed_;
}

void statement::reset() {
	sqlite3_reset(handle_);
	sqlite3_clear_bindings(handle_);
	nextParameter_ = 1;
}

long long statement::intAt(int column) const {
	return sqlite3_column_int64(handle_, column);
}

std::string statement::textAt(int column) const {
	const unsigned char* text = sqlite3_column_text(handle_, column);
	if (text == nullptr)
		return "";
	return std::string(reinterpret_cast<const char*>(text),
	                   static_cast<size_t>(sqlite3_column_bytes(handle_, column)));
}

database::~database() {
	close();
}

bool database::open(const std::filesystem::path& file, openMode mode, int busyWaitMs) {
	close();
	int flags = mode == openMode::kReadOnly ? SQLITE_OPEN_READONLY
	                                        : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	if (sqlite3_open_v2(file.c_str(), &handle_, flags, nullptr) != SQLITE_OK) {
		// A handle is returned even for a failed open, to carry the message.
		openError_ = handle_ ? sqlite3_errmsg(handle_) : "out of memory";
		close();
		return false;
	}
	sqlite3_busy_timeout(handle_, busyWaitMs);
	return true;
}

void database::close() {
	sqlite3_close(handle_);
	handle_ = nullptr;
}

bool database::run(const std::string& sql) {
	return handle_ != nullptr and
	       sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

statement database::prepare(const std::string& sql) const {
	return statement(handle_, sql);
}

long long database::lastInsertId() const {
	return handle_ ? sqlite3_last_insert_rowid(handle_) : 0;
}

std::string database::lastError() const {
	return handle_ ? sqlite3_errmsg(handle_) : openError_;
}

transaction::transaction(database& db) : db_(db), active_(db.run("BEGIN IMMEDIATE")) {}

transaction::~transaction() {
	if (active_)
		(void)db_.run("ROLLBACK");
}

bool transaction::commit() {
	if (not active_)
		return false;
	bool done = db_.run("COMMIT");
	if (not done)
		(void)db_.run("ROLLBACK");
	active_ = false;
	return done;
}

}  // namespace catalist::store
