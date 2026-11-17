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
#include "web/tracker.hpp"

namespace catalist::test {

std::filesystem::path makeUniqueTempDir(const std::string& prefix);

class tempDirFixture {
public:
	tempDirFixture();
	~tempDirFixture();
	tempDirFixture(const tempDirFixture&) = delete;
	tempDirFixture& operator=(const tempDirFixture&) = delete;

	[[nodiscard]] const std::filesystem::path& tempDir() const { return tempDir_; }

private:
	std::filesystem::path tempDir_;
};

// The text of a file, or "" when there is none.
[[nodiscard]] std::string readText(const std::filesystem::path& file);
void writeText(const std::filesystem::path& file, const std::string& text);
// Puts a directory where the file should go, so writing it fails.
void makeUnwritable(const std::filesystem::path& file);

// Answers a request whose method, URL and body together ("POST url body")
// contain a registered fragment with its reply (the longest such fragment),
// and fails every other request, as if offline. Records every request.
struct fakeWeb {
	// Requests that hold `fragment` get this reply; any other gets none, as
	// when there is no connection.
	void respond(const std::string& fragment, const std::string& body, int status = 200);
	[[nodiscard]] std::optional<web::reply> fetch(const web::request& sent);
	[[nodiscard]] web::link link();

	// "POST url body" for each request, in the order sent.
	std::vector<std::string> sent;
	// The bodies of the requests that had one.
	std::vector<std::string> posted;
	std::map<std::string, web::reply> bodies;
	// Runs at each request, as the site is being asked.
	std::function<void()> whileAsked;
};

// Test data must spell dates correctly; a typo should fail loudly instead
// of silently becoming "no date".
isoDate requireDate(const std::string& text);

class entryBuilder {
public:
	entryBuilder& id(int value) {
		built_.id = value;
		return *this;
	}
	entryBuilder& malId(int value) {
		built_.malId = value;
		return *this;
	}
	entryBuilder& anilistId(int value) {
		built_.anilistId = value;
		return *this;
	}
	entryBuilder& kitsuId(int value) {
		built_.kitsuId = value;
		return *this;
	}
	entryBuilder& title(std::string value) {
		built_.title = std::move(value);
		return *this;
	}
	entryBuilder& type(mediaType value) {
		built_.type = value;
		return *this;
	}
	entryBuilder& status(watchStatus value) {
		built_.status = value;
		return *this;
	}
	entryBuilder& score(int value) {
		built_.score = value;
		return *this;
	}
	entryBuilder& progress(int value) {
		built_.progress = value;
		return *this;
	}
	entryBuilder& totalUnits(int value) {
		built_.totalUnits = value;
		return *this;
	}
	entryBuilder& volumes(int value) {
		built_.volumes = value;
		return *this;
	}
	entryBuilder& totalVolumes(int value) {
		built_.totalVolumes = value;
		return *this;
	}
	entryBuilder& rewatched(int value) {
		built_.rewatched = value;
		return *this;
	}
	entryBuilder& startDate(const std::string& value) {
		built_.startDate = requireDate(value);
		return *this;
	}
	entryBuilder& finishDate(const std::string& value) {
		built_.finishDate = requireDate(value);
		return *this;
	}
	entryBuilder& updatedAt(const std::string& value) {
		built_.updatedAt = requireDate(value);
		return *this;
	}
	entryBuilder& released(int value) {
		built_.released = value;
		return *this;
	}
	entryBuilder& checkedAt(const std::string& value) {
		built_.checkedAt = requireDate(value);
		return *this;
	}

	[[nodiscard]] entry build() const { return built_; }
	operator entry() const { return built_; }

private:
	entry built_;
};

[[nodiscard]] entryBuilder makeEntry();

// The date every test runs on: 2026-10-20.
[[nodiscard]] isoDate testToday();
// The clock of the command-line tests: 2026-10-20 21:30:00, testToday.
[[nodiscard]] std::tm testClock();

// The entry titled `title`; fails the test when there is none.
[[nodiscard]] entry findTitle(const catalogue& db, mediaType type, const std::string& title);

// One list as the catalogue in that directory has it stored, as rows of
// csvCodec::encodeEntry ("" when nothing is stored): what a new run would
// read.
[[nodiscard]] std::string storedRows(const std::filesystem::path& dataDir, mediaType type);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
