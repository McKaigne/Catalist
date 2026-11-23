#ifndef CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/entry.hpp"
#include "storage/catalogue.hpp"
#include "web/http.hpp"

namespace catalist::test {

std::filesystem::path MakeUniqueTempDir(const std::string& prefix);

class TempDirFixture {
public:
	TempDirFixture();
	~TempDirFixture();
	TempDirFixture(const TempDirFixture&) = delete;
	TempDirFixture& operator=(const TempDirFixture&) = delete;

	[[nodiscard]] const std::filesystem::path& TempDir() const { return tempDir_; }

private:
	std::filesystem::path tempDir_;
};

// The text of a file, or "" when there is none.
[[nodiscard]] std::string ReadText(const std::filesystem::path& file);
void WriteText(const std::filesystem::path& file, const std::string& text);
// Puts a directory where the file should go, so writing it fails.
void MakeUnwritable(const std::filesystem::path& file);

// Answers a request whose method, URL and body together ("POST url body")
// contain a registered fragment with its reply (the longest such fragment),
// and fails every other request, as if offline. Records every request.
struct FakeWeb {
	// Requests that hold `fragment` get this reply; any other gets none, as
	// when there is no connection.
	void Respond(const std::string& fragment, const std::string& body, int status = 200);
	[[nodiscard]] std::optional<web::Reply> Fetch(const web::Request& sent);
	[[nodiscard]] web::Fetcher Link();

	// "POST url body" for each request, in the order sent.
	std::vector<std::string> sent;
	// The bodies of the requests that had one.
	std::vector<std::string> posted;
	std::map<std::string, web::Reply> bodies;
	// Runs at each request, as the site is being asked.
	std::function<void()> while_asked;
};

// Test data must spell dates correctly; a typo should fail loudly instead
// of silently becoming "no date".
IsoDate RequireDate(const std::string& text);

class EntryBuilder {
public:
	EntryBuilder& Id(int value) {
		built_.id = value;
		return *this;
	}
	EntryBuilder& MalId(int value) {
		built_.mal_id = value;
		return *this;
	}
	EntryBuilder& AnilistId(int value) {
		built_.anilist_id = value;
		return *this;
	}
	EntryBuilder& Title(std::string value) {
		built_.title = std::move(value);
		return *this;
	}
	EntryBuilder& Type(MediaType value) {
		built_.type = value;
		return *this;
	}
	EntryBuilder& Status(WatchStatus value) {
		built_.status = value;
		return *this;
	}
	EntryBuilder& Score(int value) {
		built_.score = value;
		return *this;
	}
	EntryBuilder& Progress(int value) {
		built_.progress = value;
		return *this;
	}
	EntryBuilder& TotalUnits(int value) {
		built_.total_units = value;
		return *this;
	}
	EntryBuilder& Volumes(int value) {
		built_.volumes = value;
		return *this;
	}
	EntryBuilder& TotalVolumes(int value) {
		built_.total_volumes = value;
		return *this;
	}
	EntryBuilder& Rewatched(int value) {
		built_.rewatched = value;
		return *this;
	}
	EntryBuilder& StartDate(const std::string& value) {
		built_.start_date = RequireDate(value);
		return *this;
	}
	EntryBuilder& FinishDate(const std::string& value) {
		built_.finish_date = RequireDate(value);
		return *this;
	}
	EntryBuilder& UpdatedAt(const std::string& value) {
		built_.updated_at = RequireDate(value);
		return *this;
	}

	[[nodiscard]] Entry Build() const { return built_; }
	operator Entry() const { return built_; }

private:
	Entry built_;
};

[[nodiscard]] EntryBuilder MakeEntry();

// The date every test runs on: 2026-10-20.
[[nodiscard]] IsoDate TestToday();
// The clock of the command-line tests: 2026-10-20 21:30:00, TestToday.
[[nodiscard]] std::tm TestClock();

// The entry titled `title`; fails the test when there is none.
[[nodiscard]] Entry FindTitle(const Catalogue& db, MediaType type, const std::string& title);

// One list as the catalogue in that directory has it stored, as rows of
// csv_codec::EncodeEntry ("" when nothing is stored): what a new run would
// read.
[[nodiscard]] std::string StoredRows(const std::filesystem::path& data_dir, MediaType type);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
