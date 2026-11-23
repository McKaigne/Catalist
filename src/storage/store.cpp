#include "storage/store.hpp"

#include <sqlite3.h>

#include <utility>

namespace catalist::store {

Statement::Statement(sqlite3* db, const std::string& sql) {
	failed_ = db == nullptr or sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()),
	                                              &handle_, nullptr) != SQLITE_OK;
}

Statement::~Statement() {
	sqlite3_finalize(handle_);
}

Statement::Statement(Statement&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)),
      nextParameter_(other.nextParameter_),
      failed_(other.failed_) {}

Statement& Statement::Bind(long long value) {
	if (not failed_)
		failed_ = sqlite3_bind_int64(handle_, nextParameter_++, value) != SQLITE_OK;
	return *this;
}

Statement& Statement::Bind(const std::string& value) {
	if (not failed_)
		failed_ = sqlite3_bind_text(handle_, nextParameter_++, value.data(),
		                            static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK;
	return *this;
}

Statement& Statement::BindNull() {
	if (not failed_)
		failed_ = sqlite3_bind_null(handle_, nextParameter_++) != SQLITE_OK;
	return *this;
}

bool Statement::Step() {
	if (failed_)
		return false;
	int state = sqlite3_step(handle_);
	failed_ = state != SQLITE_ROW and state != SQLITE_DONE;
	return state == SQLITE_ROW;
}

bool Statement::Run() {
	if (failed_)
		return false;
	failed_ = sqlite3_step(handle_) != SQLITE_DONE;
	sqlite3_reset(handle_);
	sqlite3_clear_bindings(handle_);
	nextParameter_ = 1;
	return not failed_;
}

void Statement::Reset() {
	sqlite3_reset(handle_);
	sqlite3_clear_bindings(handle_);
	nextParameter_ = 1;
}

long long Statement::IntAt(int column) const {
	return sqlite3_column_int64(handle_, column);
}

std::string Statement::TextAt(int column) const {
	const unsigned char* text = sqlite3_column_text(handle_, column);
	if (text == nullptr)
		return "";
	return std::string(reinterpret_cast<const char*>(text),
	                   static_cast<size_t>(sqlite3_column_bytes(handle_, column)));
}

Database::~Database() {
	Close();
}

bool Database::Open(const std::filesystem::path& file) {
	Close();
	int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	if (sqlite3_open_v2(file.c_str(), &handle_, flags, nullptr) != SQLITE_OK) {
		// A handle is returned even for a failed open, to carry the message.
		openError_ = handle_ ? sqlite3_errmsg(handle_) : "out of memory";
		Close();
		return false;
	}
	sqlite3_busy_timeout(handle_, 5000);
	return true;
}

void Database::Close() {
	sqlite3_close(handle_);
	handle_ = nullptr;
}

bool Database::Run(const std::string& sql) {
	return handle_ != nullptr and
	       sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

Statement Database::Prepare(const std::string& sql) const {
	return Statement(handle_, sql);
}

long long Database::LastInsertId() const {
	return handle_ ? sqlite3_last_insert_rowid(handle_) : 0;
}

std::string Database::LastError() const {
	return handle_ ? sqlite3_errmsg(handle_) : openError_;
}

Transaction::Transaction(Database& db) : db_(db), active_(db.Run("BEGIN IMMEDIATE")) {}

Transaction::~Transaction() {
	if (active_)
		(void)db_.Run("ROLLBACK");
}

bool Transaction::Commit() {
	if (not active_)
		return false;
	bool done = db_.Run("COMMIT");
	if (not done)
		(void)db_.Run("ROLLBACK");
	active_ = false;
	return done;
}

}  // namespace catalist::store
