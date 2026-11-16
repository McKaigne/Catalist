#include "support/testSupport.hpp"

#include <unistd.h>

#include <atomic>
#include <format>
#include <fstream>
#include <random>
#include <stdexcept>

#include "storage/csvCodec.hpp"
#include "system.hpp"

namespace catalist::test {

namespace {

std::string randomSuffix() {
	static std::atomic<unsigned> counter{0};
	std::random_device device;
	return std::to_string(::getpid()) + "-" + std::to_string(counter++) + "-" +
	       std::to_string(device());
}

}  // namespace

std::filesystem::path makeUniqueTempDir(const std::string& prefix) {
	auto base = std::filesystem::temp_directory_path();
	for (int attempt = 0; attempt < 16; ++attempt) {
		auto candidate = base / (prefix + randomSuffix());
		if (std::filesystem::create_directory(candidate))
			return candidate;
	}
	throw std::runtime_error("could not create a unique temp directory");
}

tempDirFixture::tempDirFixture() : tempDir_(makeUniqueTempDir("catalist-test-")) {}

tempDirFixture::~tempDirFixture() {
	std::error_code ignored;
	std::filesystem::remove_all(tempDir_, ignored);
}

isoDate requireDate(const std::string& text) {
	auto parsed = isoDate::parse(text);
	if (not parsed)
		throw std::invalid_argument("bad test date: " + text);
	return *parsed;
}

entryBuilder makeEntry() {
	return entryBuilder{};
}

isoDate testToday() {
	return requireDate("2026-10-20");
}

std::tm testClock() {
	std::tm local{};
	local.tm_year = 2026 - 1900;
	local.tm_mon = 10 - 1;
	local.tm_mday = 20;
	local.tm_hour = 21;
	local.tm_min = 30;
	return local;
}

std::string readText(const std::filesystem::path& file) {
	return readFile(file).value_or("");
}

void writeText(const std::filesystem::path& file, const std::string& text) {
	if (not file.parent_path().empty())
		std::filesystem::create_directories(file.parent_path());
	std::ofstream stream(file, std::ios::binary | std::ios::trunc);
	stream << text;
	if (not stream.good())
		throw std::runtime_error("could not write " + file.string());
}

void makeUnwritable(const std::filesystem::path& file) {
	std::filesystem::create_directories(file);
}

void fakeWeb::respond(const std::string& fragment, const std::string& body) {
	bodies[fragment] = body;
}

std::optional<std::string> fakeWeb::fetch(const web::request& asked) {
	std::string request = std::format("{} {} {}", asked.method, asked.url, asked.body);
	sent.push_back(request);
	if (not asked.body.empty())
		posted.push_back(asked.body);
	const std::string* answer = nullptr;
	size_t matched = 0;
	for (const auto& [fragment, body] : bodies) {
		if (request.find(fragment) != std::string::npos and fragment.size() >= matched) {
			answer = &body;
			matched = fragment.size();
		}
	}
	return answer ? std::optional<std::string>(*answer) : std::nullopt;
}

web::link fakeWeb::link() {
	return {[this](const web::request& asked) { return fetch(asked); }};
}

entry findTitle(const catalogue& db, mediaType type, const std::string& title) {
	for (const auto& e : db.getEntries(type)) {
		if (e.title == title)
			return e;
	}
	throw std::runtime_error("no entry titled " + title);
}

std::string storedRows(const std::filesystem::path& dataDir, mediaType type) {
	catalogue db(dataDir);
	auto loaded = db.loadAll();
	if (not loaded.has_value())
		throw std::runtime_error(loaded.error());
	std::string rows;
	for (const auto& row : db.getEntries(type))
		rows += csvCodec::encodeEntry(row) + "\n";
	return rows;
}

}  // namespace catalist::test
