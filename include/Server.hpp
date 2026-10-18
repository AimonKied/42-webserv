#pragma once

#include "Client.hpp"
#include "CgiExecutor.hpp"
#include "ConfigTypes.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <vector>
#include <fcntl.h>
#include <errno.h>

static const int CLIENT_TIMEOUT_SECONDS = 30;

namespace webserv {

class Server {
public:
	explicit Server(const Config& config);
	~Server();

	int run();

private:
	Server(const Server& other);
	Server& operator=(const Server& other);

	static void handleSignal(int signal);

	std::map<int, size_t> _listeners;
	std::chrono::steady_clock::time_point _acceptRetryAt{};
	bool isListener(int fd) const;
	int setupSignalHandlers();
	int createListeningSocket(size_t configIndex);
	int acceptClient(int listenerFd);
	int handleClientRead(size_t& i);
	int handleClientWrite(size_t& i);
	void cleanupClient(size_t& i);
	void closeAllFds();
	int setNonBlocking(int fd);
	void checkClientTimeouts();
	void collectCgiResults();
	void queueResponse(Client& client, const Response& response);


	static volatile std::sig_atomic_t _signalReceived;

	bool _running;
	const Config _config;
	CgiExecutor _cgi;
	std::vector<pollfd> _fds;
	std::map<int, Client> _clients;
};


} // namespace webserv
