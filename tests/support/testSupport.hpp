#ifndef CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
#define CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "anilist.hpp"
#include "catalogue.hpp"
#include "entry.hpp"
#include "jikan.hpp"

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

// Answers a GET whose URL contains a registered fragment with its body (the
// longest such fragment), and fails every other request, as if offline.
// Records every URL asked for.
struct fakeWeb {
	void respond(const std::string& urlFragment, const std::string& body);
	[[nodiscard]] std::optional<std::string> get(const std::string& url);
	// A POST is answered the same way, matching against the URL and the body.
	[[nodiscard]] std::optional<std::string> post(const std::string& url, const std::string& json);
	[[nodiscard]] jikan::fetcher fetcher();
	[[nodiscard]] anilist::poster poster();

	std::vector<std::string> requested;  // GETs
	std::vector<std::string> posted;     // POST bodies
	std::map<std::string, std::string> bodies;
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

// The entry titled `title`; fails the test when there is none.
[[nodiscard]] entry findTitle(const catalogue& db, mediaType type, const std::string& title);

std::filesystem::path fixturePath(const std::string& relativePath);

}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
