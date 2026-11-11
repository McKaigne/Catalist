#include "support/testSupport.hpp"

#include <unistd.h>

#include <atomic>
#include <random>
#include <stdexcept>

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

std::string readText(const std::filesystem::path& file) {
	return readFile(file).value_or("");
}

void writeText(const std::filesystem::path& file, const std::string& text) {
	if (not writeFileAtomic(file, text))
		throw std::runtime_error("could not write " + file.string());
}

void makeUnwritable(const std::filesystem::path& file) {
	std::filesystem::create_directories(file);
}

void fakeWeb::respond(const std::string& urlFragment, const std::string& body) {
	bodies[urlFragment] = body;
}

std::optional<std::string> fakeWeb::get(const std::string& url) {
	requested.push_back(url);
	// The longest matching fragment wins, so "anime/9/episodes" beats
	// "anime/9".
	const std::string* answer = nullptr;
	size_t matched = 0;
	for (const auto& [fragment, body] : bodies) {
		if (url.find(fragment) != std::string::npos and fragment.size() >= matched) {
			answer = &body;
			matched = fragment.size();
		}
	}
	return answer ? std::optional<std::string>(*answer) : std::nullopt;
}

jikan::fetcher fakeWeb::fetcher() {
	return [this](const std::string& url) { return get(url); };
}

entry findTitle(const catalogue& db, mediaType type, const std::string& title) {
	for (const auto& e : db.getEntries(type)) {
		if (e.title == title)
			return e;
	}
	throw std::runtime_error("no entry titled " + title);
}

std::filesystem::path fixturePath(const std::string& relativePath) {
	return std::filesystem::path(CATALIST_FIXTURE_DIR) / relativePath;
}

}  // namespace catalist::test
