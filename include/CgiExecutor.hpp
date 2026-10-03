#pragma once

#include <chrono>
#include <map>
#include <poll.h>
#include <string>
#include <sys/types.h>
#include <vector>

namespace webserv {

// Launcher handover: no routing or HTTP response decisions belong here.
struct CgiLaunch {
	std::string executable;
	std::string scriptPath;
	std::string workingDirectory;
	std::vector<std::string> environment;
	std::string input;
};

struct CgiResult {
	int clientFd;
	int errorCode; // 0: parse output; 502: child/pipe/output failure; 504: deadline.
	std::string output;
};

class CgiExecutor {
public:
	CgiExecutor() = default;
	~CgiExecutor();
	CgiExecutor(const CgiExecutor&) = delete;
	CgiExecutor& operator=(const CgiExecutor&) = delete;

	bool start(int clientFd, const CgiLaunch& launch);
	void appendPollFds(std::vector<pollfd>& fds) const;
	void handleEvents(const std::vector<pollfd>& ready);
	std::vector<CgiResult> collectFinished();
	void cancel(int clientFd);
	void shutdown();
	bool empty() const { return _jobs.empty(); }

private:
	struct Job {
		int clientFd = -1;
		pid_t pid = -1;
		int inputFd = -1;
		int outputFd = -1;
		std::string input;
		size_t inputOffset = 0;
		std::string output;
		std::chrono::steady_clock::time_point deadline;
		bool exited = false;
		bool outputEof = false;
		int exitStatus = 0;
		int errorCode = 0;
	};
	// PID key keeps cancelled children tracked until waitpid reaps them, even
	// when a newly accepted client reuses the old socket descriptor.
	std::map<pid_t, Job> _jobs;
	static void closeFd(int& fd);
	static void fail(Job& job, int code);
};

} // namespace webserv
