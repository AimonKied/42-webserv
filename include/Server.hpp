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
	explicit Server(int port);
	~Server();

	int run();

private:
	Server(const Server& other);
	Server& operator=(const Server& other);

	static void handleSignal(int signal);

	int setupSignalHandlers();
	int createListeningSocket();
	int acceptClient();
	int handleClientRead(size_t& i);
	int handleClientWrite(size_t& i);
	void cleanupClient(size_t& i);
	void closeAllFds();
	int setNonBlocking(int fd);
	void checkClientTimeouts();

	std::string buildResponse(const Request& request, const LocationConfig& location) const;

	static volatile std::sig_atomic_t _signalReceived;

	bool _running;
	int _port;
	int _serverFd;
	std::vector<pollfd> _fds;
	std::map<int, Client> _clients;
};

} // namespace webserv
