#pragma once

#include "Client.hpp"

#include <arpa/inet.h>
#include <csignal>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <vector>

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

	bool requestComplete(const std::string& buffer) const;
	std::string buildResponse(int clientFd) const;

	static volatile std::sig_atomic_t _signalReceived;

	bool _running;
	int _port;
	int _serverFd;
	std::vector<pollfd> _fds;
	std::map<int, Client> _clients;
};

} // namespace webserv
