#include "support/test_support.hpp"

#include <unistd.h>

#include <atomic>
#include <format>
#include <fstream>
#include <random>
#include <stdexcept>

#include "storage/csv_codec.hpp"
#include "system.hpp"

namespace catalist::test {

namespace {

std::string RandomSuffix() {
	static std::atomic<unsigned> counter{0};
	std::random_device device;
	return std::to_string(::getpid()) + "-" + std::to_string(counter++) + "-" +
	       std::to_string(device());
}

}  // namespace

std::filesystem::path MakeUniqueTempDir(const std::string& prefix) {
	auto base = std::filesystem::temp_directory_path();
	for (int attempt = 0; attempt < 16; ++attempt) {
		auto candidate = base / (prefix + RandomSuffix());
		if (std::filesystem::create_directory(candidate))
			return candidate;
	}
	throw std::runtime_error("could not create a unique temp directory");
}

TempDirFixture::TempDirFixture() : tempDir_(MakeUniqueTempDir("catalist-test-")) {}

TempDirFixture::~TempDirFixture() {
	std::error_code ignored;
	std::filesystem::remove_all(tempDir_, ignored);
}

IsoDate RequireDate(const std::string& text) {
	auto parsed = IsoDate::Parse(text);
	if (not parsed)
		throw std::invalid_argument("bad test date: " + text);
	return *parsed;
}

EntryBuilder MakeEntry() {
	return EntryBuilder{};
}

IsoDate TestToday() {
	return RequireDate("2026-10-20");
}

std::tm TestClock() {
	std::tm local{};
	local.tm_year = 2026 - 1900;
	local.tm_mon = 10 - 1;
	local.tm_mday = 20;
	local.tm_hour = 21;
	local.tm_min = 30;
	return local;
}

std::string ReadText(const std::filesystem::path& file) {
	return ReadFile(file).value_or("");
}

void WriteText(const std::filesystem::path& file, const std::string& text) {
	if (not file.parent_path().empty())
		std::filesystem::create_directories(file.parent_path());
	std::ofstream stream(file, std::ios::binary | std::ios::trunc);
	stream << text;
	if (not stream.good())
		throw std::runtime_error("could not write " + file.string());
}

void MakeUnwritable(const std::filesystem::path& file) {
	std::filesystem::create_directories(file);
}

void FakeWeb::Respond(const std::string& fragment, const std::string& body, int status) {
	bodies[fragment] = {status, body};
}

std::optional<web::Reply> FakeWeb::Fetch(const web::Request& asked) {
	std::string request = std::format("{} {} {}", asked.method, asked.url, asked.body);
	sent.push_back(request);
	if (while_asked)
		while_asked();
	if (not asked.body.empty())
		posted.push_back(asked.body);
	const web::Reply* answer = nullptr;
	size_t matched = 0;
	for (const auto& [fragment, body] : bodies) {
		if (request.find(fragment) != std::string::npos and fragment.size() >= matched) {
			answer = &body;
			matched = fragment.size();
		}
	}
	return answer ? std::optional<web::Reply>(*answer) : std::nullopt;
}

web::Link FakeWeb::Link() {
	return {[this](const web::Request& asked) { return Fetch(asked); }};
}

Entry FindTitle(const Catalogue& db, MediaType type, const std::string& title) {
	for (const auto& e : db.GetEntries(type)) {
		if (e.title == title)
			return e;
	}
	throw std::runtime_error("no entry titled " + title);
}

std::string StoredRows(const std::filesystem::path& data_dir, MediaType type) {
	Catalogue db(data_dir);
	auto loaded = db.LoadAll();
	if (not loaded.has_value())
		throw std::runtime_error(loaded.error());
	std::string rows;
	for (const auto& row : db.GetEntries(type))
		rows += csv_codec::EncodeEntry(row) + "\n";
	return rows;
}

}  // namespace catalist::test
