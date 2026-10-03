#pragma once

#include "Client.hpp"
#include "ConfigTypes.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#include <arpa/inet.h>
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

	static const size_t MAX_REQUEST_SIZE = 1024*1024;
	std::map<int, size_t> _listeners;
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


	std::string buildResponse(const Request& request, const LocationConfig& location) const;

	static volatile std::sig_atomic_t _signalReceived;

	bool _running;
	Config _config;
	std::vector<pollfd> _fds;
	std::map<int, Client> _clients;
};


} // namespace webserv