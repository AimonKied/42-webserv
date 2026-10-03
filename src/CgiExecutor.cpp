#include "CgiExecutor.hpp"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace webserv {
namespace {
const size_t MAX_CGI_OUTPUT = 4 * 1024 * 1024;
const int CGI_TIMEOUT_SECONDS = 5;

bool cloexec(int fd) {
	return fcntl(fd, F_SETFD, FD_CLOEXEC) != -1;
}

bool nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}
}

CgiExecutor::~CgiExecutor() { shutdown(); }

void CgiExecutor::closeFd(int& fd) {
	if (fd >= 0) close(fd);
	fd = -1;
}

void CgiExecutor::fail(Job& job, int code) {
	if (!job.errorCode) job.errorCode = code;
	closeFd(job.inputFd);
	closeFd(job.outputFd);
	if (!job.exited) kill(job.pid, SIGKILL);
}

bool CgiExecutor::start(int clientFd, const CgiLaunch& launch) {
	// Allocate all C++ storage before fork. Parent endpoints alone are nonblocking;
	// ordinary CGI scripts expect blocking stdin/stdout.
	Job job;
	job.clientFd = clientFd;
	job.input = launch.input;
	job.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(CGI_TIMEOUT_SECONDS);
	std::vector<char*> env;
	for (const std::string& entry : launch.environment)
		env.push_back(const_cast<char*>(entry.c_str()));
	env.push_back(nullptr);
	char* argv[] = {const_cast<char*>(launch.executable.c_str()),
					const_cast<char*>(launch.scriptPath.c_str()), nullptr};
	int input[2] = {-1, -1};
	int output[2] = {-1, -1};
	if (pipe(input) == -1) return false;
	if (pipe(output) == -1) {
		close(input[0]); close(input[1]);
		return false;
	}
	// Reserve descriptors above stderr; this avoids dup2/close aliasing if the
	// server was started with one of its standard descriptors closed.
	bool valid = true;
	for (int* fd : {&input[0], &input[1], &output[0], &output[1]}) {
		if (*fd <= STDERR_FILENO) {
			int replacement = fcntl(*fd, F_DUPFD, STDERR_FILENO + 1);
			close(*fd);
			*fd = replacement;
		}
		if (*fd < 0 || !cloexec(*fd)) valid = false;
	}
	if (!valid || !nonblocking(input[1]) || !nonblocking(output[0])) {
		for (int fd : {input[0], input[1], output[0], output[1]})
			if (fd >= 0) close(fd);
		return false;
	}
	pid_t pid = fork();
	if (pid == 0) {
		std::signal(SIGPIPE, SIG_DFL);
		if (dup2(input[0], STDIN_FILENO) == -1 ||
			dup2(output[1], STDOUT_FILENO) == -1 ||
			chdir(launch.workingDirectory.c_str()) == -1)
			_exit(126);
		close(input[0]); close(input[1]); close(output[0]); close(output[1]);
		execve(launch.executable.c_str(), argv, env.data());
		_exit(127);
	}
	close(input[0]); close(output[1]);
	if (pid == -1) {
		close(input[1]); close(output[0]);
		return false;
	}
	job.pid = pid;
	job.inputFd = input[1];
	job.outputFd = output[0];
	if (job.input.empty()) closeFd(job.inputFd); // deliver EOF even for GET
	try {
		_jobs.emplace(pid, std::move(job));
	} catch (...) {
		closeFd(job.inputFd); closeFd(job.outputFd);
		kill(pid, SIGKILL);
		while (waitpid(pid, nullptr, 0) == -1 && errno == EINTR) {}
		throw;
	}
	return true;
}

void CgiExecutor::appendPollFds(std::vector<pollfd>& fds) const {
	for (const auto& entry : _jobs) {
		const Job& job = entry.second;
		if (job.inputFd >= 0) fds.push_back({job.inputFd, POLLOUT, 0});
		if (job.outputFd >= 0) fds.push_back({job.outputFd, POLLIN, 0});
	}
}

void CgiExecutor::handleEvents(const std::vector<pollfd>& ready) {
	for (auto& entry : _jobs) {
		Job& job = entry.second;
		for (const pollfd& event : ready) {
			if (!event.revents || job.errorCode) continue;
			if (event.fd == job.inputFd) {
				if (event.revents & (POLLERR | POLLHUP | POLLNVAL)) {
					// A CGI may deliberately ignore its input; retain its output.
					closeFd(job.inputFd);
				} else if (event.revents & POLLOUT) {
					ssize_t count = write(job.inputFd, job.input.data() + job.inputOffset,
										  job.input.size() - job.inputOffset);
					if (count <= 0) { fail(job, 502); continue; }
					job.inputOffset += static_cast<size_t>(count);
					if (job.inputOffset == job.input.size()) {
						closeFd(job.inputFd);
						job.input.clear();
					}
				}
			} else if (event.fd == job.outputFd) {
				if (event.revents & (POLLERR | POLLNVAL)) { fail(job, 502); continue; }
				// HUP can coexist with unread data. Drain over subsequent poll
				// turns until read returns zero; never discard that final data.
				if (event.revents & (POLLIN | POLLHUP)) {
					char buffer[16384];
					ssize_t count = read(job.outputFd, buffer, sizeof(buffer));
					if (count < 0) { fail(job, 502); continue; }
					if (count == 0) {
						job.outputEof = true;
						closeFd(job.outputFd);
					} else if (static_cast<size_t>(count) > MAX_CGI_OUTPUT - job.output.size()) {
						fail(job, 502);
					} else {
						job.output.append(buffer, static_cast<size_t>(count));
					}
				}
			}
		}
	}
}

std::vector<CgiResult> CgiExecutor::collectFinished() {
	std::vector<CgiResult> results;
	const auto now = std::chrono::steady_clock::now();
	for (auto it = _jobs.begin(); it != _jobs.end();) {
		Job& job = it->second;
		if (!job.exited) {
			pid_t result = waitpid(job.pid, &job.exitStatus, WNOHANG);
			if (result == job.pid) {
				job.exited = true;
				if (!WIFEXITED(job.exitStatus) || WEXITSTATUS(job.exitStatus) != 0)
					fail(job, 502);
			} else if (result == -1 && errno != EINTR) {
				job.exited = true;
				fail(job, 502);
			}
		}
		if (!job.errorCode && now >= job.deadline && !(job.exited && job.outputEof))
			fail(job, 504);
		if (job.exited && (job.errorCode || job.outputEof)) {
			closeFd(job.inputFd); closeFd(job.outputFd);
			if (job.clientFd >= 0)
				results.push_back({job.clientFd, job.errorCode, std::move(job.output)});
			it = _jobs.erase(it);
		} else ++it;
	}
	return results;
}

void CgiExecutor::cancel(int clientFd) {
	for (auto& entry : _jobs) {
		Job& job = entry.second;
		if (job.clientFd == clientFd) {
			job.clientFd = -1;
			fail(job, 502);
		}
	}
}

void CgiExecutor::shutdown() {
	for (auto& entry : _jobs) fail(entry.second, 502);
	// Only teardown waits synchronously, after every live child has been killed.
	for (auto& entry : _jobs) {
		Job& job = entry.second;
		if (!job.exited)
			while (waitpid(job.pid, nullptr, 0) == -1 && errno == EINTR) {}
	}
	_jobs.clear();
}
} // namespace webserv
