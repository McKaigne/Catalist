#include "system.hpp"

#include <fcntl.h>
#include <fmt/format.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <thread>
#include <utility>

namespace catalist {

std::optional<std::string> readFile(const std::filesystem::path& file) {
	// A directory opens as a stream on Linux but throws when read.
	std::error_code ec;
	if (not std::filesystem::is_regular_file(file, ec))
		return std::nullopt;
	std::ifstream stream(file, std::ios::binary);
	if (not stream.is_open())
		return std::nullopt;
	return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

bool writeFileAtomic(const std::filesystem::path& file, const std::string& content) {
	std::error_code ec;
	if (not file.parent_path().empty())
		std::filesystem::create_directories(file.parent_path(), ec);
	std::filesystem::path tempFile = fmt::format("{}.tmp", file.string());
	bool written = false;
	{
		std::ofstream stream(tempFile, std::ios::binary | std::ios::trunc);
		stream << content;
		stream.flush();
		written = stream.is_open() and stream.good();
	}
	if (written)
		std::filesystem::rename(tempFile, file, ec);
	if (written and not ec)
		return true;
	std::filesystem::remove(tempFile, ec);
	return false;
}

namespace {

// Owns a file descriptor and closes it when it goes out of scope.
class fileDescriptor {
public:
	explicit fileDescriptor(int descriptor = -1) : descriptor_(descriptor) {}
	~fileDescriptor() { reset(); }
	fileDescriptor(fileDescriptor&& other) noexcept
	    : descriptor_(std::exchange(other.descriptor_, -1)) {}
	fileDescriptor& operator=(fileDescriptor&& other) noexcept {
		reset();
		descriptor_ = std::exchange(other.descriptor_, -1);
		return *this;
	}
	fileDescriptor(const fileDescriptor&) = delete;
	fileDescriptor& operator=(const fileDescriptor&) = delete;

	[[nodiscard]] int get() const { return descriptor_; }
	void reset() {
		if (descriptor_ >= 0)
			close(descriptor_);
		descriptor_ = -1;
	}

private:
	int descriptor_;
};

struct pipeEnds {
	fileDescriptor read;
	fileDescriptor write;
};

// Both ends close themselves in a child once it runs another program.
std::optional<pipeEnds> makePipe() {
	int ends[2];
	if (pipe(ends) != 0)
		return std::nullopt;
	for (int end : ends)
		fcntl(end, F_SETFD, FD_CLOEXEC);
	return pipeEnds{fileDescriptor(ends[0]), fileDescriptor(ends[1])};
}

}  // namespace

std::optional<std::string> runProgram(const std::vector<std::string>& arguments,
                                      const std::string& input) {
	auto toChild = makePipe();
	auto fromChild = makePipe();
	if (not toChild or not fromChild or arguments.empty())
		return std::nullopt;

	pid_t child = fork();
	if (child == 0) {
		// In the child: the pipes become standard input and output, errors go
		// nowhere, and every other descriptor closes on exec.
		dup2(toChild->read.get(), STDIN_FILENO);
		dup2(fromChild->write.get(), STDOUT_FILENO);
		fileDescriptor devNull(open("/dev/null", O_WRONLY | O_CLOEXEC));
		if (devNull.get() >= 0)
			dup2(devNull.get(), STDERR_FILENO);
		std::vector<char*> argv;
		std::transform(
		    arguments.begin(), arguments.end(), std::back_inserter(argv),
		    [](const std::string& argument) { return const_cast<char*>(argument.c_str()); });
		argv.push_back(nullptr);
		execvp(argv[0], argv.data());
		_exit(127);
	}
	// In the parent: only our ends stay open.
	toChild->read.reset();
	fromChild->write.reset();
	if (child < 0)
		return std::nullopt;

	// Feed the input from another thread, so a program that answers before it
	// has read everything cannot block on a full pipe.
	std::thread feeder([&input, end = std::move(toChild->write)] {
		size_t written = 0;
		while (written < input.size()) {
			ssize_t count = write(end.get(), input.data() + written, input.size() - written);
			if (count <= 0)
				break;
			written += static_cast<size_t>(count);
		}
	});

	// Collect everything the program writes until it closes its output.
	std::string output;
	std::array<char, 4096> buffer;
	ssize_t count = 0;
	while ((count = read(fromChild->read.get(), buffer.data(), buffer.size())) > 0)
		output.append(buffer.data(), static_cast<size_t>(count));
	feeder.join();

	int status = 0;
	waitpid(child, &status, 0);
	bool succeeded = WIFEXITED(status) and WEXITSTATUS(status) == 0;
	return succeeded ? std::optional<std::string>(output) : std::nullopt;
}

bool hasProgram(const std::string& name) {
	const char* path = std::getenv("PATH");
	std::string directories = path ? path : "";
	size_t start = 0;
	while (start <= directories.size()) {
		size_t end = directories.find(':', start);
		if (end == std::string::npos)
			end = directories.size();
		std::filesystem::path candidate =
		    std::filesystem::path(directories.substr(start, end - start)) / name;
		if (access(candidate.c_str(), X_OK) == 0)
			return true;
		start = end + 1;
	}
	return false;
}

std::optional<std::string> httpGet(const std::string& url) {
	// -f turns an HTTP error into a failed exit, -m bounds the whole request so
	// nothing waits longer than that.
	return runProgram({"curl", "-sfL", "-m", "8", "-A", "catalist", url});
}

isoDate today() {
	std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm local{};
	localtime_r(&seconds, &local);
	return isoDate::fromLocalTm(local);
}

std::optional<cellSize> terminalCellSize() {
	winsize window{};
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0 or window.ws_col == 0 or
	    window.ws_row == 0 or window.ws_xpixel == 0 or window.ws_ypixel == 0)
		return std::nullopt;
	return cellSize{window.ws_xpixel / window.ws_col, window.ws_ypixel / window.ws_row};
}

}  // namespace catalist
