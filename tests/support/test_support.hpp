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

[[nodiscard]] std::string ReadText(const std::filesystem::path& file);
void WriteText(const std::filesystem::path& file, const std::string& text);

struct FakeWeb {
	void Respond(const std::string& fragment, const std::string& body, int status = 200);
	[[nodiscard]] std::optional<web::Reply> Fetch(const web::Request& sent);
	[[nodiscard]] web::Fetcher Link();

	std::map<std::string, web::Reply> bodies;
};

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
	EntryBuilder& EnglishTitle(std::string value) {
		built_.english_title = std::move(value);
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
	EntryBuilder& Format(catalist::MediaFormat value) {
		built_.format = value;
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

[[nodiscard]] IsoDate TestToday();
[[nodiscard]] std::tm TestClock();

[[nodiscard]] Entry FindTitle(const Catalogue& db, MediaType type, const std::string& title);


}  // namespace catalist::test

#endif  // CATALIST_TESTS_SUPPORT_TEST_SUPPORT_HPP_
