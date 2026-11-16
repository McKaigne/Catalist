#include "system.hpp"

#include <fcntl.h>
#include <fmt/format.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <istream>
#include <iterator>
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

	// Feed the input and collect the output together, so a program that
	// answers before it has read everything cannot block on a full pipe.
	fileDescriptor feed = std::move(toChild->write);
	fcntl(feed.get(), F_SETFL, O_NONBLOCK);
	size_t written = 0;
	if (input.empty())
		feed.reset();
	std::string output;
	std::array<char, 4096> buffer;
	while (true) {
		// poll passes over the input's end once it is closed (-1).
		std::array<pollfd, 2> ends = {pollfd{fromChild->read.get(), POLLIN, 0},
		                              pollfd{feed.get(), POLLOUT, 0}};
		if (poll(ends.data(), ends.size(), -1) < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (ends[1].revents != 0) {
			ssize_t count = write(feed.get(), input.data() + written, input.size() - written);
			if (count > 0)
				written += static_cast<size_t>(count);
			if ((count < 0 and errno != EAGAIN) or written == input.size())
				feed.reset();
		}
		if (ends[0].revents != 0) {
			ssize_t count = read(fromChild->read.get(), buffer.data(), buffer.size());
			if (count <= 0)
				break;
			output.append(buffer.data(), static_cast<size_t>(count));
		}
	}
	feed.reset();

	int status = 0;
	waitpid(child, &status, 0);
	bool succeeded = WIFEXITED(status) and WEXITSTATUS(status) == 0;
	return succeeded ? std::optional<std::string>(output) : std::nullopt;
}

namespace {

// -f turns an HTTP error into a failed exit; a server that does not answer
// is given up on after four seconds, and the whole request after eight. -g
// leaves the brackets of a URL (Kitsu's filter[text]) to the server.
const std::vector<std::string> kCurl = {"curl", "-sfLg", "--connect-timeout", "4", "-m",
                                        "8",    "-A",    "catalist"};

}  // namespace

std::optional<std::string> httpFetch(const web::request& sent) {
	std::vector<std::string> arguments = kCurl;
	if (sent.method != "GET")
		arguments.insert(arguments.end(), {"-X", sent.method});
	for (const auto& header : sent.headers)
		arguments.insert(arguments.end(), {"-H", header});
	// The body goes in on standard input, never on the command line.
	if (not sent.body.empty())
		arguments.insert(arguments.end(), {"--data-binary", "@-"});
	arguments.push_back(sent.url);
	return runProgram(arguments, sent.body);
}

isoDate today() {
	std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm local{};
	localtime_r(&seconds, &local);
	return isoDate::fromLocalTm(local);
}

std::string nowStamp() {
	std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm local{};
	localtime_r(&seconds, &local);
	return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", local.tm_year + 1900,
	                   local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

bool isInteractive() {
	return isatty(STDIN_FILENO) and isatty(STDOUT_FILENO);
}

int terminalColumns() {
	winsize window{};
	if (not isatty(STDOUT_FILENO) or ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) != 0)
		return 0;
	return window.ws_col;
}

bool writePrivateFile(const std::filesystem::path& file, const std::string& text) {
	std::error_code ec;
	if (file.has_parent_path())
		std::filesystem::create_directories(file.parent_path(), ec);
	// Written beside it and moved in place, so a crash leaves the old file.
	std::filesystem::path next = file;
	next += ".new";
	fileDescriptor out(open(next.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600));
	if (out.get() < 0 or fchmod(out.get(), 0600) != 0)
		return false;
	for (size_t done = 0; done < text.size();) {
		ssize_t count = write(out.get(), text.data() + done, text.size() - done);
		if (count <= 0)
			return false;
		done += static_cast<size_t>(count);
	}
	if (fsync(out.get()) != 0)
		return false;
	out.reset();
	std::filesystem::rename(next, file, ec);
	return not ec;
}

std::optional<std::string> readSecretLine(std::istream& in) {
	termios before{};
	bool hidden = isatty(STDIN_FILENO) and tcgetattr(STDIN_FILENO, &before) == 0;
	if (hidden) {
		termios quiet = before;
		quiet.c_lflag &= static_cast<tcflag_t>(~ECHO);
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet);
	}
	std::string line;
	bool read = static_cast<bool>(std::getline(in, line));
	if (hidden) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &before);
		// The Enter that was not shown.
		std::fputs("\n", stderr);
	}
	return read ? std::optional<std::string>(line) : std::nullopt;
}

std::string randomText(size_t length) {
	static const char kLetters[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
	std::string bytes(length, '\0');
	std::ifstream source("/dev/urandom", std::ios::binary);
	source.read(bytes.data(), static_cast<std::streamsize>(length));
	std::string text;
	for (unsigned char c : bytes)
		text += kLetters[c % (sizeof(kLetters) - 1)];
	return text;
}

long long unixNow() {
	return static_cast<long long>(
	    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

}  // namespace catalist
