#pragma once

#include "Client.hpp"

#include <arpa/inet.h>
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

	int createListeningSocket();
	int acceptClient();
	int handleClientRead(size_t& i);
	int handleClientWrite(size_t& i);
	void cleanupClient(size_t& i);

	bool requestComplete(const std::string& buffer) const;
	std::string buildResponse(int clientFd) const;

	int _port;
	int _serverFd;
	std::vector<pollfd> _fds;
	std::map<int, Client> _clients;
};

} // namespace webserv
