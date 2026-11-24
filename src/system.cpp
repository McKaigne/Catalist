#include "system.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <utility>

namespace catalist {

std::optional<std::string> ReadFile(const std::filesystem::path& file) {
	// A directory opens as a stream on Linux but throws when read.
	std::error_code ec;
	if (std::filesystem::is_directory(file, ec))
		return std::nullopt;
	std::ifstream stream(file, std::ios::binary);
	if (not stream.is_open())
		return std::nullopt;
	return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

bool WriteFile(const std::filesystem::path& file, const std::string& text) {
	// Written beside it and renamed, so a crash cannot leave half a file.
	auto partial = file;
	partial += ".tmp";
	std::ofstream stream(partial, std::ios::binary | std::ios::trunc);
	stream << text;
	stream.close();
	std::error_code ec;
	if (stream.good())
		std::filesystem::rename(partial, file, ec);
	if (stream.good() and not ec)
		return true;
	std::filesystem::remove(partial, ec);
	return false;
}

std::string ExpandHome(const std::string& path) {
	const char* home = std::getenv("HOME");
	bool tilde = path == "~" or path.starts_with("~/");
	return tilde and home ? home + path.substr(1) : path;
}

namespace {

class FileDescriptor {
public:
	explicit FileDescriptor(int descriptor = -1) : descriptor_(descriptor) {}
	~FileDescriptor() { Reset(); }
	FileDescriptor(FileDescriptor&& other) noexcept
	    : descriptor_(std::exchange(other.descriptor_, -1)) {}

	[[nodiscard]] int Get() const { return descriptor_; }
	void Reset() {
		if (descriptor_ >= 0)
			close(descriptor_);
		descriptor_ = -1;
	}

private:
	int descriptor_;
};

struct PipeEnds {
	FileDescriptor read;
	FileDescriptor write;
};

std::optional<PipeEnds> MakePipe() {
	int ends[2];
	if (pipe(ends) != 0)
		return std::nullopt;
	for (int end : ends)
		fcntl(end, F_SETFD, FD_CLOEXEC);
	return PipeEnds{FileDescriptor(ends[0]), FileDescriptor(ends[1])};
}

}  // namespace

std::optional<std::string> RunProgram(const std::vector<std::string>& arguments,
                                      const std::string& input) {
	auto to_child = MakePipe();
	auto from_child = MakePipe();
	if (not to_child or not from_child or arguments.empty())
		return std::nullopt;

	std::vector<char*> argv;
	argv.reserve(arguments.size() + 1);
	for (const std::string& argument : arguments)
		argv.push_back(const_cast<char*>(argument.c_str()));
	argv.push_back(nullptr);

	pid_t child = fork();
	if (child == 0) {
		dup2(to_child->read.Get(), STDIN_FILENO);
		dup2(from_child->write.Get(), STDOUT_FILENO);
		FileDescriptor dev_null(open("/dev/null", O_WRONLY | O_CLOEXEC));
		if (dev_null.Get() >= 0)
			dup2(dev_null.Get(), STDERR_FILENO);
		execvp(argv[0], argv.data());
		_exit(127);
	}
	to_child->read.Reset();
	from_child->write.Reset();
	if (child < 0)
		return std::nullopt;

	// A program that exits before reading its input fails the request; the
	// write must not kill Catalist.
	auto previous_handler = std::signal(SIGPIPE, SIG_IGN);

	// Feed the input and collect the output together, so a program that
	// answers before it has read everything cannot block on a full pipe.
	FileDescriptor feed = std::move(to_child->write);
	fcntl(feed.Get(), F_SETFL, O_NONBLOCK);
	size_t written = 0;
	if (input.empty())
		feed.Reset();
	std::string output;
	std::array<char, 4096> buffer;
	while (true) {
		// poll passes over the input's end once it is closed (-1).
		std::array<pollfd, 2> ends = {pollfd{from_child->read.Get(), POLLIN, 0},
		                              pollfd{feed.Get(), POLLOUT, 0}};
		if (poll(ends.data(), ends.size(), -1) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (ends[1].revents != 0) {
			ssize_t count = write(feed.Get(), input.data() + written, input.size() - written);
			if (count > 0)
				written += static_cast<size_t>(count);
			if ((count < 0 and errno != EAGAIN and errno != EINTR) or written == input.size())
				feed.Reset();
		}
		if (ends[0].revents != 0) {
			ssize_t count = read(from_child->read.Get(), buffer.data(), buffer.size());
			if (count < 0 and errno == EINTR)
				continue;
			if (count <= 0)
				break;
			output.append(buffer.data(), static_cast<size_t>(count));
		}
	}
	feed.Reset();
	std::signal(SIGPIPE, previous_handler);

	int status = 0;
	pid_t waited = -1;
	do {
		waited = waitpid(child, &status, 0);
	} while (waited < 0 and errno == EINTR);
	bool succeeded = waited == child and WIFEXITED(status) and WEXITSTATUS(status) == 0;
	return succeeded ? std::optional<std::string>(output) : std::nullopt;
}

namespace {

// The status goes on a line of its own after the body (-w), so an error's
// body is kept too; a server that does not answer is given up on after four
// seconds, and the whole request after eight.
const std::vector<std::string> kCurl = {"curl", "-sL", "--connect-timeout", "4",  "-m",
                                        "8",    "-A",  "catalist",          "-w", "\n%{http_code}"};

}  // namespace

std::optional<web::Reply> HttpFetch(const web::Request& sent) {
	std::vector<std::string> arguments = kCurl;
	for (const auto& header : sent.headers)
		arguments.insert(arguments.end(), {"-H", header});
	// Without -X, -L would turn the POST into a GET after a redirect.
	arguments.insert(arguments.end(), {"-X", "POST", "--data-binary", "@-", sent.url});
	auto output = RunProgram(arguments, sent.body);
	size_t line_end = output ? output->rfind('\n') : std::string::npos;
	if (line_end == std::string::npos)
		return std::nullopt;
	int status = 0;
	const char* first = output->data() + line_end + 1;
	const char* last = output->data() + output->size();
	auto [end, error] = std::from_chars(first, last, status);
	if (error != std::errc() or end != last or status == 0)
		return std::nullopt;
	return web::Reply{status, output->substr(0, line_end)};
}

std::tm LocalTime() {
	std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm local{};
	localtime_r(&seconds, &local);
	return local;
}

bool IsInteractive() {
	return isatty(STDIN_FILENO) and isatty(STDOUT_FILENO);
}

int TerminalColumns() {
	winsize window{};
	if (not isatty(STDOUT_FILENO) or ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0)
		return 0;
	return window.ws_col;
}

}  // namespace catalist
